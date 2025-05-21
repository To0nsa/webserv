#include "http/handleCgi.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/stringUtils.hpp"

#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <poll.h>
#include <signal.h>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

/**
 * @brief Resolves the absolute path to a CGI script from a request path.
 *
 * @details Strips the location prefix from the request path (if it matches),
 * then appends the resulting relative path to the given root directory,
 * returning the absolute filesystem path to the CGI script to execute.
 *
 * @param requestPath    Full request URI path received from the client (e.g. "/cgi-bin/hello.py").
 * @param locationPrefix The configured location path (e.g. "/cgi-bin").
 * @param root           Filesystem root where scripts are located (e.g. "./www/scripts").
 *
 * @return Absolute path to the resolved script as a std::filesystem::path.
 *
 * @ingroup http
 */
std::filesystem::path resolveScriptPath(const std::string& requestPath,
                                        const std::string& locationPrefix,
                                        const std::string& root) {
    std::string relative = requestPath;

    // If the request path starts with the location prefix, strip it off
    if (requestPath.compare(0, locationPrefix.size(), locationPrefix) == 0) {
        relative = requestPath.substr(locationPrefix.size());

        // If the remaining relative path starts with '/', remove it
        if (!relative.empty() && relative[0] == '/')
            relative.erase(0, 1);
    }

    // Join root and relative path, then convert to absolute path
    return std::filesystem::absolute(std::filesystem::path(root) / relative);
}

/**
 * @brief Converts HTTP headers to CGI-compliant environment variables.
 *
 * @details Iterates through the provided HTTP headers, transforms each header
 * name by converting it to uppercase and replacing hyphens with underscores,
 * then prepends "HTTP_" and appends the value, storing the result in the
 * `env` vector as expected by the CGI standard.
 *
 * Example: "Content-Type: text/plain" → "HTTP_CONTENT_TYPE=text/plain"
 *
 * @param env     Output vector to store environment variable strings.
 * @param headers Map of HTTP header fields to be transformed.
 *
 * @ingroup http
 */
void appendHttpHeadersAsEnvVars(std::vector<std::string>&                 env,
                                const std::map<std::string, std::string>& headers) {
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it) {
        std::string key = it->first;

        // Convert header name to uppercase
        std::transform(key.begin(), key.end(), key.begin(), ::toupper);

        // Replace '-' with '_' to match CGI env var format
        std::replace(key.begin(), key.end(), '-', '_');

        // Prepend "HTTP_" and append the header value
        env.push_back("HTTP_" + key + "=" + it->second);
    }
}

/**
 * @brief Prepares the environment variables for CGI execution.
 *
 * @details This function populates the `env` vector with the standard set of
 * environment variables required by the CGI specification (RFC 3875).
 * It includes request metadata (method, path, query), server details,
 * script location, and headers. HTTP headers are added using the
 * `appendHttpHeadersAsEnvVars` helper.
 *
 * @param env         Output vector of environment variable strings in the format "KEY=VALUE".
 * @param req         HTTP request object containing method, path, query, headers, etc.
 * @param server      Server object to extract host and port information.
 * @param loc         Matched Location configuration providing the document root.
 * @param scriptPath  Absolute filesystem path to the CGI script being executed.
 *
 * @ingroup http
 */
void prepareEnvironment(std::vector<std::string>& env, const HttpRequest& req, const Server& server,
                        const Location& loc, const std::filesystem::path& scriptPath) {
    // Helper lambda to append environment variables
    auto set = [&](const std::string& k, const std::string& v) { env.push_back(k + "=" + v); };

    // CGI standard variables
    set("REQUEST_METHOD", req.getMethod()); // GET, POST, etc.
    set("SCRIPT_NAME", req.getPath());      // Original request path
    set("PATH_INFO", scriptPath);           // Filesystem path to script
    set("QUERY_STRING", req.getQuery());    // Part after '?'

    // Optional content headers (if present)
    if (!req.getHeader("Content-Length").empty())
        set("CONTENT_LENGTH", req.getHeader("Content-Length"));

    if (!req.getHeader("Content-Type").empty())
        set("CONTENT_TYPE", req.getHeader("Content-Type"));

    // Protocol and server metadata
    set("SERVER_PROTOCOL", "HTTP/1.1");                   // Fixed as per RFC
    set("GATEWAY_INTERFACE", "CGI/1.1");                  // Required by CGI
    set("SERVER_SOFTWARE", "webserv/1.0");                // Custom server identifier
    set("DOCUMENT_ROOT", loc.getRoot());                  // Base root for the route
    set("SERVER_NAME", server.getDefaultServerName());    // Hostname
    set("SERVER_PORT", std::to_string(server.getPort())); // Listening port

    // System PATH variable for executable lookup
    set("PATH", "/usr/bin:/bin:/usr/local/bin");

    // Add HTTP headers as CGI-compliant environment variables
    appendHttpHeadersAsEnvVars(env, req.getHeaders());
}

/**
 * @brief Converts a vector of strings to a null-terminated array of C-style char* pointers.
 *
 * @details This is required when calling functions like `execve()` which expect
 * arguments and environment variables in the form of `char*[]`.
 * The resulting array is null-terminated, as expected by POSIX APIs.
 *
 * Important: This function uses `const_cast` to strip constness. It is safe here
 * because the strings in `vec` remain alive and are not modified.
 *
 * @param vec Input vector of std::string.
 * @return A vector of `char*` with a trailing `nullptr` element.
 *
 * @ingroup utils
 */
std::vector<char*> toCharPtrArray(const std::vector<std::string>& vec) {
    std::vector<char*> out;

    // Push each string's underlying C-string into the output array
    for (const auto& s : vec)
        out.push_back(const_cast<char*>(s.c_str()));

    // Append null terminator as required by execve
    out.push_back(nullptr);
    return out;
}

/**
 * @brief Reads from a file descriptor with timeout using poll().
 *
 * @details Monitors a file descriptor (typically the stdout of a CGI child process)
 * for readability using `poll()`. If the descriptor becomes readable within the
 * timeout, it reads available data into the output buffer. On timeout or error,
 * the child process is killed and the function returns false.
 *
 * @param fd      File descriptor to read from (e.g. pipe from CGI stdout).
 * @param pid     PID of the child process to terminate on timeout/error.
 * @param output  Output string to append read data to.
 *
 * @return `true` if the read completed normally, `false` on timeout or error.
 *
 * @ingroup http
 */
bool readWithPoll(int fd, pid_t pid, std::string& output) {
    const int timeoutMs = 2000; // 2 seconds timeout
    pollfd    pfd       = {fd, POLLIN, 0};
    char      buffer[4096];

    while (true) {
        // Wait until fd is readable or timeout occurs
        int ret = poll(&pfd, 1, timeoutMs);

        if (ret == 0) {
            std::cerr << "[CGI] Timeout\n";
            kill(pid, SIGKILL);       // Kill hung child
            waitpid(pid, nullptr, 0); // Reap the zombie
            return false;
        } else if (ret < 0) {
            perror("[CGI] poll error");
            kill(pid, SIGKILL); // Kill child on error
            waitpid(pid, nullptr, 0);
            return false;
        }

        // Check if fd has data to read
        if (pfd.revents & POLLIN) {
            ssize_t n = read(fd, buffer, sizeof(buffer));
            if (n > 0) {
                output.append(buffer, n); // Append read bytes to output
            } else if (n == 0) {
                break; // EOF reached
            } else {
                perror("[CGI] read error");
                return false;
            }
        } else {
            // Unexpected event (not POLLIN), treat as done
            break;
        }
    }

    return true;
}

/**
 * @brief Parses CGI response headers and extracts key fields.
 *
 * @details This function reads CGI-style headers from a raw string and extracts
 * the `Status`, `Content-Type`, and `Location` fields. The `Status` field may include
 * a numeric code and an optional message. Keys are case-insensitive and normalized to lowercase.
 *
 * Example input:
 * ```
 * Content-Type: text/plain
 * Status: 404 Not Found
 * Location: /redirect
 * ```
 *
 * @param headers       Raw header string (CRLF-separated or LF-separated).
 * @param statusCode    Output HTTP status code (default: 200 if not present).
 * @param statusMessage Output status message (default: "OK" if not present).
 * @param contentType   Output MIME type (e.g., "text/html").
 * @param location      Output redirect location, if any.
 *
 * @ingroup http
 */
void parseHeaders(const std::string& headers, int& statusCode, std::string& statusMessage,
                  std::string& contentType, std::string& location) {
    std::istringstream iss(headers);

    // Iterate through each line of the headers
    for (std::string line; std::getline(iss, line);) {
        size_t sep = line.find(':');
        if (sep == std::string::npos)
            continue; // Skip lines without a colon

        // Split key and value, trimming whitespace
        std::string key   = trim(line.substr(0, sep));
        std::string value = trim(line.substr(sep + 1));

        // Normalize header key to lowercase
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        if (key == "content-type") {
            contentType = value;
        } else if (key == "status") {
            std::istringstream s(value);
            s >> statusCode;                // Extract numeric status code
            std::getline(s, statusMessage); // Extract optional message
            statusMessage = trim(statusMessage);
        } else if (key == "location") {
            location = value;
        }
    }
}

/**
 * @brief Parses a CGI response output into a structured HttpResponse.
 *
 * @details Splits the CGI output into headers and body using the standard CRLF delimiter.
 * It then extracts key header values (e.g., `Content-Type`, `Status`, `Location`) via
 * `parseHeaders()`, builds an `HttpResponse`, and sets the appropriate fields.
 * If a `Location` header is found with a 200 status, it automatically transforms it
 * into a 302 redirect as per CGI spec behavior.
 *
 * @param output  Raw string returned by the CGI script (headers + body).
 * @param server  Reference to the `Server` context for fallback/error responses.
 * @param request Original HTTP request (used for response construction).
 *
 * @return Fully constructed `HttpResponse` object.
 *
 * @ingroup http
 */
HttpResponse parseCgiResponse(std::string& output, const Server& server,
                              const HttpRequest& request) {
    // Separate headers and body using standard CGI delimiter
    size_t pos = output.find("\r\n\r\n");
    if (pos == std::string::npos) {
        // Malformed CGI response: no header/body separator
        return ResponseBuilder::generateError(500, server, request);
    }

    std::string headers = output.substr(0, pos);
    std::string body    = output.substr(pos + 4);

    // Defaults if not overridden by CGI headers
    std::string contentType   = "text/plain";
    std::string statusMessage = "OK";
    std::string location;
    int         statusCode = 200;

    // Extract key headers (Content-Type, Status, Location)
    parseHeaders(headers, statusCode, statusMessage, contentType, location);

    // If CGI returned Location but no redirect status, convert to 302
    if (statusCode == 200 && !location.empty()) {
        statusCode    = 302;
        statusMessage = "Found";
    }

    // Build final HttpResponse object
    HttpResponse response =
        ResponseBuilder::generateSuccess(statusCode, body, contentType, request);

    if (!statusMessage.empty())
        response.setStatus(statusCode, statusMessage);
    if (!location.empty())
        response.setHeader("Location", location);

    return response;
}

/**
 * @brief Reads and parses the output of a CGI child process.
 *
 * @details Uses `readWithPoll` to safely read the CGI process's stdout with timeout handling.
 * After reading, it waits for the child to exit and validates its exit status.
 * If the child exited normally with code 0, the output is parsed into an `HttpResponse`.
 * Otherwise, appropriate 5xx errors are returned.
 *
 * @param fd      File descriptor to read CGI output from (stdout of child).
 * @param pid     PID of the CGI child process.
 * @param server  Reference to the current server instance (for error context).
 * @param request HTTP request associated with this CGI call.
 *
 * @return A valid `HttpResponse` from the CGI output, or an error response.
 *
 * @ingroup http
 */
HttpResponse readCgiOutput(int fd, pid_t pid, const Server& server, const HttpRequest& request) {
    std::string output;

    // Read CGI output with poll and timeout protection
    if (!readWithPoll(fd, pid, output)) {
        return ResponseBuilder::generateError(500, server, request);
    }

    int status = 0;

    // Wait for the CGI process to exit
    if (waitpid(pid, &status, 0) < 0) {
        perror("[CGI] waitpid failed");
        return ResponseBuilder::generateError(502, server, request);
    }

    // Ensure child exited normally (not killed by signal)
    if (!WIFEXITED(status)) {
        std::cerr << "[CGI] CGI process did not exit normally (signal?)\n";
        return ResponseBuilder::generateError(502, server, request);
    }

    // Check that exit code is 0 (success)
    if (WEXITSTATUS(status) != 0) {
        std::cerr << "[CGI] CGI exited with status " << WEXITSTATUS(status) << "\n";
        return ResponseBuilder::generateError(502, server, request);
    }

    // Parse the CGI response (headers + body) into HttpResponse
    return parseCgiResponse(output, server, request);
}

/**
 * @brief Executes a CGI script in the child process.
 *
 * @details Sets up the input/output redirection for the CGI process using `dup2`,
 * prepares the environment variables, changes the working directory to the script's
 * parent directory, and finally invokes the script using `execve()`.
 *
 * If any step fails, the child process prints an error and exits with status code 1.
 *
 * @param request     The HTTP request to pass to the CGI script.
 * @param server      The associated Server instance (for env vars like SERVER_NAME).
 * @param location    The matched Location block (for DOCUMENT_ROOT, etc.).
 * @param scriptPath  Absolute path to the CGI script.
 * @param pipeIn      Pipe for reading input from parent (pipeIn[0] → stdin).
 * @param pipeOut     Pipe for writing output to parent (stdout → pipeOut[1]).
 *
 * @ingroup http
 */
void runChildCgi(const HttpRequest& request, const Server& server, const Location& location,
                 const std::filesystem::path& scriptPath, int pipeIn[2], int pipeOut[2]) {
    // Redirect stdin to pipeIn[0] and stdout to pipeOut[1]
    if (dup2(pipeIn[0], STDIN_FILENO) < 0 || dup2(pipeOut[1], STDOUT_FILENO) < 0) {
        perror("[CGI] dup2 failed");
        exit(1); // Fatal in child process
    }

    // Close unused pipe ends
    close(pipeIn[1]);  // Only reading from pipeIn[0]
    close(pipeOut[0]); // Only writing to pipeOut[1]

    // Prepare CGI environment variables
    std::vector<std::string> env;
    prepareEnvironment(env, request, server, location, scriptPath);

    // Change working directory to script's parent (for relative paths)
    std::filesystem::path dir = scriptPath.parent_path();
    if (chdir(dir.c_str()) != 0) {
        perror("[CGI] chdir failed");
        exit(1);
    }

    // Prepare arguments and environment for execve
    std::string        absPath = scriptPath.string();
    std::vector<char*> argv    = {const_cast<char*>(absPath.c_str()), nullptr};
    std::vector<char*> envp    = toCharPtrArray(env);

    // Execute the CGI script
    execve(argv[0], argv.data(), envp.data());

    // If execve returns, it's an error
    perror("[CGI] execve failed");
    exit(1);
}

/**
 * @brief Sends the HTTP request body to the CGI child process via pipe.
 *
 * @details This function is executed in the parent process after forking the CGI child.
 * It writes the request body (only for POST requests) to the child's stdin through
 * the provided pipe, using `poll()` to ensure non-blocking behavior and timeout safety.
 *
 * If writing fails or times out, the loop exits early. The write-end of the pipe is
 * always closed afterward to signal EOF to the child.
 *
 * @param request HTTP request containing the body to be sent.
 * @param pipeIn  Pipe to the CGI child's stdin (write to pipeIn[1]).
 *
 * @ingroup http
 */
void handleParentCgi(const HttpRequest& request, int pipeIn[2]) {
    if (request.getMethod() == "POST") {
        const char* body      = request.getBody().c_str();
        size_t      remaining = request.getBody().size();
        const int   timeoutMs = 2000;

        // Write loop with poll timeout protection
        while (remaining > 0) {
            pollfd pfd = {pipeIn[1], POLLOUT, 0};
            int    ret = poll(&pfd, 1, timeoutMs);

            if (ret == 0) {
                std::cerr << "[CGI] write timeout\n";
                break;
            } else if (ret < 0) {
                perror("[CGI] poll write failed");
                break;
            }

            // If pipe is ready for writing
            if (pfd.revents & POLLOUT) {
                ssize_t written = write(pipeIn[1], body, remaining);
                if (written < 0) {
                    if (errno == EINTR)
                        continue; // Retry on signal interruption
                    perror("[CGI] write failed");
                    break;
                }
                body += written;
                remaining -= written;
            } else {
                std::cerr << "[CGI] unexpected poll write event\n";
                break;
            }
        }
    }

    // Signal EOF to the CGI process
    close(pipeIn[1]);
}

} // namespace

/**
 * @brief Handles execution of a CGI script and returns its response.
 *
 * @details Resolves the script path, validates it, sets up communication pipes,
 * forks a child process to run the script, and manages data exchange between
 * the parent and child. It builds an appropriate HttpResponse from the CGI output.
 *
 * @param request   HTTP request triggering the CGI.
 * @param server    Associated server block (for error context).
 * @param location  Matching location block (for script root).
 * @return          HttpResponse built from the CGI output or an error response.
 */
HttpResponse handleCgi(const HttpRequest& request, const Server& server, const Location& location) {
    // Resolve the full filesystem path of the CGI script to execute
    std::filesystem::path scriptPath =
        resolveScriptPath(request.getPath(), location.getPath(), location.getRoot());

    // Return 404 if the script file doesn't exist
    if (!isFile(scriptPath)) {
        std::cerr << "[CGI] script not found: " << scriptPath << "\n";
        return ResponseBuilder::generateError(404, server, request);
    }

    // Return 403 if the script isn't executable
    if (access(scriptPath.c_str(), X_OK) != 0) {
        std::cerr << "[CGI] script not executable: " << scriptPath << "\n";
        return ResponseBuilder::generateError(403, server, request);
    }

    int pipeIn[2], pipeOut[2];

    // Create pipes: parent → child (stdin), child → parent (stdout)
    if (pipe(pipeIn) < 0 || pipe(pipeOut) < 0) {
        perror("[CGI] pipe failed");
        return ResponseBuilder::generateError(500, server, request);
    }

    // Fork a new process to run the CGI script
    pid_t pid = fork();
    if (pid < 0) {
        perror("[CGI] fork failed");
        return ResponseBuilder::generateError(500, server, request);
    }

    if (pid == 0) {
        // In child: redirect I/O and exec the CGI script
        runChildCgi(request, server, location, scriptPath, pipeIn, pipeOut);
    }

    // In parent: close pipe ends not used by parent
    close(pipeIn[0]);  // Parent doesn't read from pipeIn
    close(pipeOut[1]); // Parent doesn't write to pipeOut

    // Send POST body to child CGI process if needed
    handleParentCgi(request, pipeIn);

    // Read CGI output and return parsed HttpResponse
    return readCgiOutput(pipeOut[0], pid, server, request);
}
