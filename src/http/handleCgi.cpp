/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleCgi.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/24 12:23:37 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/19 11:32:52 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    handleCgi.cpp
 * @brief   Implements CGI (Common Gateway Interface) request handling.
 *
 * @details
 * This module provides full lifecycle management of CGI processes:
 * - Build CGI environment variables from the incoming `HttpRequest`.
 * - Create temporary input/output files to stream request/response bodies.
 * - Fork and exec the target CGI script with the correct interpreter.
 * - Collect, parse, and forward CGI output as an `HttpResponse`.
 * - Handle errors, timeouts, and cleanup of child processes.
 *
 * ### Workflow
 * 1. **initCgiProcess**: validate script, create temp files, fork child, exec interpreter.
 * 2. **Child process**: stdin → request body, stdout → temp output, `execve()` CGI script.
 * 3. **Parent process**: monitor PID, wait for completion, enforce timeouts.
 * 4. **finalizeCgi**: parse CGI headers (Status, Content-Type), build `HttpResponse`.
 * 5. **cleanup**: delete temp files and reset `CgiProcess` state.
 *
 * ### Key Functions
 * - `prepareEnv` → Build CGI environment (SCRIPT_NAME, PATH_INFO, QUERY_STRING, etc.).
 * - `prepareCgiTempFiles` → Write request body to temp file, create output file.
 * - `setupAndRunCgiChild` → Fork/exec child process safely.
 * - `finalizeCgi` → Read output, parse headers, and generate HTTP response.
 * - `errorOnCgi`, `cleanupCgi`, `tryTerminateCgi` → Robust process management.
 *
 * ### Limitations
 * - Blocking I/O (temp files, fork/exec).
 * - Only basic CGI spec (no FastCGI, no async streaming).
 *
 * @ingroup request_handler
 */

#include "http/handleCgi.hpp"
#include "core/Location.hpp"         // for Location
#include "core/Server.hpp"           // for Server
#include "http/HttpRequest.hpp"      // for HttpRequest
#include "http/responseBuilder.hpp"  // for generateError, generateSuccessFile
#include "utils/Logger.hpp"          // for LogLevel, Logger
#include "utils/filesystemUtils.hpp" // for getCurrentTime, make_temp_name
#include "utils/stringUtils.hpp"     // for trim, toUpper
#include <algorithm>                 // for replace
#include <errno.h>                   // for errno
#include <fcntl.h>                   // for open, O_CREAT, O_RDONLY, O_RDWR
#include <filesystem>                // for path, absolute, remove
#include <fstream>                   // for basic_ifstream, basic_istream
#include <map>                       // for map, operator==, _Rb_tree_const...
#include <optional>                  // for optional, nullopt
#include <poll.h>                    // for pollfd
#include <signal.h>                  // for kill, SIGKILL
#include <sstream>                   // for basic_istringstream
#include <stdlib.h>                  // for exit
#include <string.h>                  // for strerror
#include <sys/wait.h>                // for waitpid, WNOHANG
#include <system_error>              // for error_code
#include <unistd.h>                  // for close, dup2, STDIN_FILENO, STDO...
#include <utility>                   // for pair
#include <vector>                    // for vector

namespace {

/**
 * @brief Build the environment variables required for a CGI script execution.
 *
 * @details
 * Constructs a `std::vector<std::string>` of `KEY=VALUE` pairs following the CGI/1.1
 * specification. The environment includes:
 * - Standard CGI variables (`SCRIPT_NAME`, `PATH_INFO`, `REQUEST_METHOD`, etc.).
 * - Server metadata (`SERVER_NAME`, `SERVER_PORT`, `SERVER_PROTOCOL`, etc.).
 * - Request headers, prefixed with `HTTP_` and normalized (uppercased, `-` → `_`).
 * - Content metadata (`CONTENT_LENGTH`, `CONTENT_TYPE`).
 *
 * Special handling:
 * - Computes `PATH_INFO` if the request URI extends beyond the script’s URI.
 * - Sets `REDIRECT_STATUS=200` for PHP compatibility.
 *
 * @param req        Incoming HTTP request.
 * @param server     Server configuration (name, port, defaults).
 * @param loc        Matched location block (provides root, CGI settings).
 * @param scriptPath Absolute filesystem path of the CGI script to run.
 *
 * @return A vector of strings representing the CGI environment, suitable for `execve()`.
 *
 * @ingroup http_component
 */
std::vector<std::string> prepareEnv(const HttpRequest& req, const Server& server,
                                    const Location& loc, const std::string& scriptPath) {
    std::vector<std::string> env;
    auto set = [&](const std::string& k, const std::string& v) { env.push_back(k + "=" + v); };

    // Normalize paths and extract script filename
    std::string requestPath  = normalizePath(req.getPath());
    std::string locationPath = normalizePath(loc.getPath());
    std::string scriptName   = std::filesystem::path(scriptPath).filename().string();

    // Build SCRIPT_NAME and PATH_INFO
    std::string scriptUri = locationPath;
    if (!scriptUri.empty() && scriptUri.back() != '/')
        scriptUri += "/";
    scriptUri += scriptName;

    // Extract extra path after the script (PATH_INFO)
    std::string pathInfo;
    if (requestPath.size() > scriptUri.size() &&
        requestPath.compare(0, scriptUri.size(), scriptUri) == 0) {
        pathInfo = requestPath.substr(scriptUri.size());
        if (!pathInfo.empty() && pathInfo[0] != '/')
            pathInfo = "/" + pathInfo;
    }

    // Core CGI variables (script + path info)
    set("SCRIPT_NAME", req.getPath());
    if (pathInfo.empty()) {
        set("PATH_INFO", req.getPath());
    } else {
        set("PATH_INFO", pathInfo);
    }

    // Request metadata
    set("REQUEST_METHOD", req.getMethod());
    set("QUERY_STRING", req.getQuery());
    set("CONTENT_LENGTH", std::to_string(req.getContentLength()));
    if (!req.getHeader("Content-Type").empty())
        set("CONTENT_TYPE", req.getHeader("Content-Type"));

    // Server and protocol metadata
    set("SERVER_PROTOCOL", "HTTP/1.1");
    set("GATEWAY_INTERFACE", "CGI/1.1");
    set("SERVER_SOFTWARE", "webserv/1.0");
    set("DOCUMENT_ROOT", loc.getRoot());
    set("SERVER_NAME", server.getDefaultServerName());
    set("SERVER_PORT", std::to_string(server.getPort()));

    // Script execution context
    set("PATH_TRANSLATED", scriptPath);
    set("REMOTE_ADDR", "127.0.0.1"); // currently hardcoded
    set("REQUEST_URI", req.getPath());
    set("SCRIPT_FILENAME", scriptPath);
    set("REDIRECT_STATUS", "200"); // required by some CGI implementations (e.g. PHP)

    // Forward all HTTP headers as CGI variables (HTTP_FOO_BAR format)
    for (const auto& [key, value] : req.getHeaders()) {
        std::string envKey = "HTTP_" + toUpper(key);
        std::replace(envKey.begin(), envKey.end(), '-', '_');
        set(envKey, value);
    }

    return env;
}

/**
 * @brief Convert a vector of strings into a null-terminated `char*` array.
 *
 * @details
 * Prepares data for system calls like `execve()`, which require `char* argv[]`
 * or `char* envp[]` format. Each string’s `c_str()` is cast away from `const`
 * (safe here because the lifetime is managed by the original `std::string`
 * vector). The returned array is null-terminated as required by POSIX.
 *
 * @param vs Input vector of strings.
 * @return A `std::vector<char*>` containing pointers to each string, ending with `nullptr`.
 *
 * @note The caller must ensure that the original `std::vector<std::string>` outlives
 *       this array, since the pointers are non-owning.
 */
std::vector<char*> toCharPtrArray(const std::vector<std::string>& vs) {
    std::vector<char*> out;
    out.reserve(vs.size() + 1);
    for (const auto& s : vs)
        out.push_back(const_cast<char*>(s.c_str()));
    out.push_back(nullptr);
    return out;
}

/**
 * @brief Read up to a maximum number of bytes from a file stream.
 *
 * @details
 * Allocates a temporary buffer of size `maxBytes`, attempts to read from the
 * given input file stream, and returns both:
 * - The bytes read, wrapped into a `std::string`.
 * - The actual count of bytes read (`gcount()`).
 *
 * This is typically used to capture the initial chunk of a CGI output file
 * in order to parse headers before streaming the rest of the response.
 *
 * @param file     Open input file stream (must be in binary mode).
 * @param maxBytes Maximum number of bytes to read from the stream.
 *
 * @return A pair: `(stringData, bytesRead)`.
 *
 * @note If fewer than `maxBytes` are available, only the available bytes are read.
 * @warning The file’s read position will advance by the number of bytes consumed.
 */
std::pair<std::string, std::streamsize> readInitialOutput(std::ifstream& file, size_t maxBytes) {
    std::vector<char> buffer(maxBytes);
    file.read(buffer.data(), maxBytes);
    std::streamsize bytesRead = file.gcount();
    return {std::string(buffer.data(), bytesRead), bytesRead};
}

/**
 * @brief Locate the end of the HTTP header section in a CGI response.
 *
 * @details
 * Searches for the standard header delimiter:
 * - First tries `"\r\n\r\n"` (CRLF-terminated headers).
 * - Falls back to `"\n\n"` if CRLF is not found.
 *
 * Updates `delimiterLength` with the length of the matched delimiter
 * (either 4 or 2).
 *
 * @param data            Input string containing CGI response data.
 * @param delimiterLength Output; set to the delimiter size (4 or 2) if found.
 *
 * @return The position of the delimiter if found, otherwise `std::nullopt`.
 */
std::optional<size_t> findHeaderDelimiter(const std::string& data, size_t& delimiterLength) {
    size_t pos      = data.find("\r\n\r\n");
    delimiterLength = 4;
    if (pos == std::string::npos) {
        pos             = data.find("\n\n");
        delimiterLength = 2;
    }
    if (pos != std::string::npos) {
        return std::optional<size_t>(pos);
    }
    return std::nullopt;
}

/**
 * @brief Parse CGI response headers for status code and content type.
 *
 * @details
 * Iterates over each line in the provided header block:
 * - Extracts the value of `Content-Type:` if present.
 * - Extracts the numeric value of `Status:` if present (default = 200).
 *   - If parsing fails, logs a warning and falls back to `500`.
 *
 * Other headers are ignored by this function.
 *
 * @param header Raw header section of the CGI output (up to the header delimiter).
 *
 * @return A pair `(statusCode, contentType)`:
 *   - `statusCode` → HTTP status (int).
 *   - `contentType` → MIME type string (empty if not provided).
 */
std::pair<int, std::string> parseHeaders(const std::string& header) {
    std::istringstream stream(header);
    std::string        line, contentType = "";
    int                statusCode = 200;

    while (std::getline(stream, line)) {
        if (line.find("Content-Type:") == 0)
            contentType = trim(line.substr(13));
        else if (line.find("Status:") == 0) {
            try {
                statusCode = std::stoi(trim(line.substr(7)));
            } catch (...) {
                Logger::logFrom(LogLevel::WARN, "CGI", "Invalid status code in CGI response");
                statusCode = 500;
            }
        }
    }
    return {statusCode, contentType};
}

bool validateCgiScript(const std::filesystem::path& path, int& errorCode) {
    if (!isFile(path)) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "File is invalid");
        errorCode = 404;
        return false;
    }
    if (access(path.c_str(), X_OK) != 0) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "File is not executable");
        errorCode = 403;
        return false;
    }
    return true;
}

/**
 * @brief Validate that a CGI script exists and is executable.
 *
 * @details
 * Performs two checks on the given filesystem path:
 * 1. Verifies the path refers to a regular file (`isFile`).
 *    - If not, sets `errorCode = 404` (Not Found).
 * 2. Verifies the file has execute permissions (`access(..., X_OK)`).
 *    - If not, sets `errorCode = 403` (Forbidden).
 *
 * Logs errors if validation fails.
 *
 * @param path      Filesystem path to the CGI script.
 * @param errorCode Output; set to `404` or `403` on failure.
 *
 * @return true if the script exists and is executable, false otherwise.
 */
bool prepareCgiTempFiles(CgiProcess& cgi, const HttpRequest& req, int& bodyFd, int& outputFd) {
    static unsigned counter = 0;
    cgi.input_path          = make_temp_name("webserv_in", counter);
    cgi.output_path         = make_temp_name("webserv_out", counter);

    std::ofstream out(cgi.input_path, std::ios::binary);
    if (!out.is_open()) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Failed to create temp input file");
        return false;
    }
    out.write(req.getBody().data(), req.getBody().size());
    out.close();

    bodyFd = open(cgi.input_path.c_str(), O_RDONLY);
    if (bodyFd < 0) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Failed to reopen temp_in file for CGI input");
        return false;
    }

    outputFd = open(cgi.output_path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (outputFd < 0) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Failed to create CGI output file");
        close(bodyFd);
        return false;
    }

    return true;
}

/**
 * @brief Setup file descriptors, environment, and execute a CGI script in the child process.
 *
 * @details
 * This function is called after `fork()` in the CGI child. It:
 * 1. Redirects `stdin` from the temporary body file (`dup2(body_fd, STDIN_FILENO)`).
 * 2. Redirects `stdout` to the temporary output file (`dup2(output_fd, STDOUT_FILENO)`).
 * 3. Closes all unrelated file descriptors to avoid leaks.
 * 4. Resolves the CGI interpreter from the script extension (if configured).
 * 5. Builds the `argv` array for `execve()` (`[interpreter?, scriptPath, NULL]`).
 * 6. Prepares the CGI environment variables with `prepareEnv()`.
 * 7. Changes working directory to the script’s directory for relative file access.
 * 8. Calls `execve()` to replace the process image with the CGI script.
 *
 * If any step fails, logs the error and terminates the child with `exit(1)`.
 *
 * @param cgi       CGI process metadata (script path, temp files).
 * @param body_fd   File descriptor for the request body (temp input file).
 * @param output_fd File descriptor for CGI output (temp output file).
 * @param req       Incoming HTTP request (provides headers, body, method).
 * @param server    Server configuration (name, port, etc.).
 * @param loc       Location block configuration (provides CGI interpreter).
 * @param poll_fds  Active poll descriptors (closed to avoid leakage in the child).
 *
 * @warning Must only be called in the child process after `fork()`.
 * @note On success, this function never returns (replaced by `execve()`).
 */
void setupAndRunCgiChild(const CgiProcess& cgi, int body_fd, int output_fd, const HttpRequest& req,
                         const Server& server, const Location& loc,
                         const std::vector<pollfd>& poll_fds) {
    if (dup2(body_fd, STDIN_FILENO) == -1) {
        Logger::logFrom(LogLevel::ERROR, "CGI CHILD",
                        "dup2 stdin failed: " + std::string(strerror(errno)));
        exit(1);
    }
    close(body_fd);
    if (dup2(output_fd, STDOUT_FILENO) == -1) {
        Logger::logFrom(LogLevel::ERROR, "CGI CHILD",
                        "dup2 stdout failed: " + std::string(strerror(errno)));
        exit(1);
    }
    close(output_fd);
    for (std::vector<pollfd>::const_iterator it = poll_fds.begin(); it != poll_fds.end(); ++it) {
        int fd = it->fd;
        if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
            close(fd);
        }
    }

    // 1. Store script and interpreter in scoped std::string
    std::string scriptPath = cgi.script_path;
    std::string ext        = std::filesystem::path(scriptPath).extension().string();
    std::string interp     = loc.getCgiInterpreter(ext);

    // 2. Build argv using references to scoped strings
    std::vector<std::string> argvStorage;
    if (!interp.empty()) {
        argvStorage.push_back(interp);
    }
    argvStorage.push_back(scriptPath);

    std::vector<char*> argv;
    for (size_t i = 0; i < argvStorage.size(); ++i) {
        argv.push_back(const_cast<char*>(argvStorage[i].c_str()));
    }
    argv.push_back(nullptr);

    // 3. Environment: same principle
    std::vector<std::string> envStrs = prepareEnv(req, server, loc, scriptPath);
    std::vector<char*>       envp    = toCharPtrArray(envStrs);

    // 4. chdir safely
    const std::string cgiDir = std::filesystem::path(scriptPath).parent_path().string();
    if (chdir(cgiDir.c_str()) != 0) {
        Logger::logFrom(LogLevel::ERROR, "CGI CHILD",
                        "chdir failed: " + std::string(strerror(errno)));
        exit(1);
    }
    execve(argv[0], argv.data(), envp.data());

    // 5. If execve fails
    Logger::logFrom(LogLevel::ERROR, "CGI CHILD", "execve failed: " + std::string(strerror(errno)));
    exit(1);
}

} // namespace

namespace CGI {

/**
 * @brief Remove a temporary CGI file and log an error if the deletion fails.
 *
 * @details
 * Attempts to delete the given file path using `std::filesystem::remove`.
 * - If `path` is empty, nothing is done.
 * - If removal fails, logs an error including the context label and
 *   the system-provided error message.
 *
 * Commonly used for cleaning up CGI input/output temporary files.
 *
 * @param path    Filesystem path of the file to remove.
 * @param context Description of the file’s role (e.g. "input temp file").
 */
void unlinkWithErrorLog(const std::string& path, const std::string& context) {
    if (!path.empty()) {
        std::error_code ec;
        if (!std::filesystem::remove(path, ec)) {
            Logger::logFrom(LogLevel::ERROR, "CGI",
                            "Failed to delete " + context + ": " + path + " (" + ec.message() +
                                ")");
        }
    }
}

/**
 * @brief Initialize and start a CGI process for a given request.
 *
 * @details
 * - Resolves the script path from the request URI and validates it with `validateCgiScript`.
 * - Prepares temporary input/output files (`prepareCgiTempFiles`):
 *   - Writes the request body to a temp input file.
 *   - Opens a temp output file for capturing CGI output.
 * - Forks the process:
 *   - In the child → calls `setupAndRunCgiChild()` (redirects FDs, sets env, execve).
 *   - In the parent → closes temp file descriptors and stores process metadata.
 * - On error (invalid script, temp file failure, fork failure), sets `errorCode`
 *   appropriately and returns `false`.
 *
 * @param cgi       Reference to a `CgiProcess` struct (will be populated with PID, paths,
 * timestamps).
 * @param req       Incoming HTTP request (provides path, headers, body).
 * @param server    Server configuration (name, port, etc.).
 * @param loc       Location block configuration (provides root, CGI settings).
 * @param poll_fds  Current poll file descriptors (passed to child for cleanup).
 * @param errorCode Output parameter; set to HTTP-like error codes (`404`, `403`, `500`) on failure.
 *
 * @return true if the CGI process was successfully started, false otherwise.
 *
 * @note On success, the child process never returns (replaced by `execve()`).
 * @warning Must be followed by later cleanup (`finalizeCgi`, `errorOnCgi`, or `cleanupCgi`).
 */
bool initCgiProcess(CgiProcess& cgi, const HttpRequest& req, const Server& server,
                    const Location& loc, const std::vector<pollfd>& poll_fds, int& errorCode) {
    cgi.last_activity = getCurrentTime();
    cgi.script_path   = std::filesystem::absolute(loc.resolveAbsolutePath(req.getPath()));
    if (!validateCgiScript(cgi.script_path, errorCode)) {
        return false;
    }

    int body_fd = -1, output_fd = -1;
    if (!prepareCgiTempFiles(cgi, req, body_fd, output_fd)) {
        errorCode = 500;
        return false;
    }
    cgi.last_activity = getCurrentTime();

    pid_t pid = fork();
    if (pid < 0) {
        close(body_fd);
        close(output_fd);
        return false;
    }

    if (pid == 0) {
        setupAndRunCgiChild(cgi, body_fd, output_fd, req, server, loc, poll_fds);
    }

    close(body_fd);
    close(output_fd);

    cgi.pid           = pid;
    cgi.start_time    = getCurrentTime();
    cgi.last_activity = getCurrentTime();
    return true;
}

/**
 * @brief Finalize a CGI process by parsing its output and building an HTTP response.
 *
 * @details
 * - Opens the CGI output file produced by the child process.
 * - Scans up to the first 9KB (`MAX_HEADER_SCAN`) to locate the HTTP header delimiter.
 *   - If not found, logs an error and returns a `500` response.
 * - Extracts and parses CGI headers:
 *   - `Status:` → HTTP status code (default = 200).
 *   - `Content-Type:` → MIME type (optional).
 * - Computes the offset where the body starts and its size.
 * - Builds an `HttpResponse` via `ResponseBuilder::generateSuccessFile`, which streams
 *   the body directly from the output file.
 * - Marks the response with the CGI temp file path so it can be cleaned up later.
 *
 * @param cgi    Reference to the `CgiProcess` containing CGI state (output path, timestamps).
 * @param server Server configuration (used for error responses).
 * @param req    Original HTTP request (used for logging and response metadata).
 *
 * @return An `HttpResponse` containing either:
 *   - The CGI result (with headers + body).
 *   - Or an error response (500) if parsing/reading failed.
 *
 * @note Only the first 9KB of output is scanned for headers. Larger headers are unsupported.
 * @warning Caller is responsible for eventually cleaning up the CGI temp file.
 */
HttpResponse finalizeCgi(CgiProcess& cgi, const Server& server, const HttpRequest& req) {

    cgi.last_activity = getCurrentTime();
    std::ifstream in(cgi.output_path, std::ios::binary);
    if (!in.is_open()) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Failed to open CGI output file");
        return ResponseBuilder::generateError(500, server, req);
    }

    in.seekg(0, std::ios::end);
    std::streamsize totalSize = in.tellg();
    in.seekg(0, std::ios::beg);

    // Read the first 9KB only to find headers
    constexpr size_t MAX_HEADER_SCAN = 9 * 1024;
    auto [initialData, bytesRead]    = readInitialOutput(in, MAX_HEADER_SCAN);
    size_t delimLen                  = 0;
    auto   headerEndOpt              = findHeaderDelimiter(initialData, delimLen);
    if (!headerEndOpt) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Header delimiter not found in first 9KB");
        return ResponseBuilder::generateError(500, server, req);
    }

    size_t      headerPos     = *headerEndOpt;
    std::string headerSection = initialData.substr(0, headerPos);
    auto [code, contentType]  = parseHeaders(headerSection);

    std::streamsize headerEnd = static_cast<std::streamsize>(headerPos + delimLen);
    std::streamsize bodySize  = totalSize - headerEnd;
    in.close();
    req.printRequest();
    HttpResponse resp = ResponseBuilder::generateSuccessFile(code, cgi.output_path, contentType,
                                                             req, bodySize, headerEnd);
    resp.setCgiTempFile(cgi.output_path);
    return resp;
}

/**
 * @brief Forcefully terminate a CGI process and clean up its resources.
 *
 * @details
 * - Logs the intent to kill the CGI process.
 * - Sends `SIGKILL` to the child process using its PID.
 * - Calls `waitpid()` to reap the process and avoid zombies.
 *   - Logs an error if `waitpid` fails.
 * - Deletes temporary input/output files via `unlinkWithErrorLog`.
 * - Resets the `CgiProcess` state (`pid`, timestamps, paths).
 *
 * Typically invoked when a CGI process times out or becomes unresponsive.
 *
 * @param cgi Reference to the CGI process metadata to be cleaned up.
 *
 * @warning This is a hard kill (`SIGKILL`) — no graceful shutdown of the CGI script.
 */
void errorOnCgi(CgiProcess& cgi) {
    Logger::logFrom(LogLevel::ERROR, "CGI",
                    "Killing CGI process with PID: " + std::to_string(cgi.pid));
    kill(cgi.pid, SIGKILL);
    if (waitpid(cgi.pid, nullptr, 0) == -1) {
        Logger::logFrom(LogLevel::ERROR, "CGI",
                        "waitpid failed after killing CGI process: " +
                            std::string(strerror(errno)));
    }
    unlinkWithErrorLog(cgi.input_path, "input temp file");
    unlinkWithErrorLog(cgi.output_path, "output temp file");

    cgi.pid           = -1;
    cgi.start_time    = 0;
    cgi.last_activity = 0;
    cgi.input_path.clear();
    cgi.script_path.clear();
}

/**
 * @brief Clean up resources of a finished CGI process.
 *
 * @details
 * - Removes the temporary input file associated with the CGI process.
 * - Resets process metadata (`pid`, timestamps, input path).
 * - Does not touch the CGI output file (which may still be in use
 *   by the HTTP response).
 *
 * Typically called after `finalizeCgi()` has built the response.
 *
 * @param cgi Reference to the CGI process metadata to be reset.
 */
void cleanupCgi(CgiProcess& cgi) {
    unlinkWithErrorLog(cgi.input_path, "input temp file");
    cgi.pid           = -1;
    cgi.start_time    = 0;
    cgi.last_activity = 0;
    cgi.input_path.clear();
}

/**
 * @brief Check whether a CGI process has terminated, and reap it if so.
 *
 * @details
 * - Calls `waitpid(pid, &status, WNOHANG)`:
 *   - Returns `false` if the process is still running (`result == 0`).
 *   - Returns `true` if the process has terminated (or if `waitpid` fails).
 * - Logs an error if `waitpid` itself fails (`result == -1`).
 *
 * This function is non-blocking and can be safely called in the event loop
 * to periodically check if a CGI process is done.
 *
 * @param cgi Reference to the CGI process metadata.
 *
 * @return `true` if the CGI process has terminated (or an error occurred),
 *         `false` if it is still running.
 */
bool tryTerminateCgi(CgiProcess& cgi) {
    int   status;
    pid_t result = waitpid(cgi.pid, &status, WNOHANG);

    if (result == 0) {
        return false;
    }

    if (result == -1) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "waitpid failed: " + std::string(strerror(errno)));
        return true;
    }
    return true;
}

} // namespace CGI
