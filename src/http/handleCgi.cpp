/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleCgi.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/24 12:23:37 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/28 11:52:09 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handleCgi.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "utils/Logger.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/stringUtils.hpp"

#include <fcntl.h>
#include <filesystem>
#include <poll.h>
#include <signal.h>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

std::vector<std::string> prepareEnv(const HttpRequest& req, const Server& server,
                                    const Location& loc, const std::string& scriptPath) {
    std::vector<std::string> env;
    auto set = [&](const std::string& k, const std::string& v) { env.push_back(k + "=" + v); };

    std::string requestPath  = normalizePath(req.getPath());
    std::string locationPath = normalizePath(loc.getPath());
    std::string scriptName   = std::filesystem::path(scriptPath).filename().string();

    // SCRIPT_NAME = URL path to the script (/directory/youpi.bla)
    std::string scriptUri = locationPath;
    if (!scriptUri.empty() && scriptUri.back() != '/')
        scriptUri += "/";
    scriptUri += scriptName;

    // PATH_INFO = remainder of the path after SCRIPT_NAME
    std::string pathInfo;
    if (requestPath.rfind(scriptUri, 0) == 0 && requestPath.size() > scriptUri.size()) {
        pathInfo = requestPath.substr(scriptUri.size());
        if (!pathInfo.empty() && pathInfo.front() != '/')
            pathInfo.insert(pathInfo.begin(), '/');
    }
    if (pathInfo.empty())
        pathInfo = "/";

    set("REQUEST_METHOD", req.getMethod());
    set("SCRIPT_NAME", scriptUri);
    set("PATH_INFO", pathInfo);
    set("QUERY_STRING", req.getQuery());
    if (!req.getHeader("Content-Length").empty())
        set("CONTENT_LENGTH", req.getHeader("Content-Length"));
    if (!req.getHeader("Content-Type").empty())
        set("CONTENT_TYPE", req.getHeader("Content-Type"));

    set("SERVER_PROTOCOL", "HTTP/1.1");
    set("GATEWAY_INTERFACE", "CGI/1.1");
    set("SERVER_SOFTWARE", "webserv/1.0");
    set("DOCUMENT_ROOT", loc.getRoot());
    set("SERVER_NAME", server.getDefaultServerName());
    set("SERVER_PORT", std::to_string(server.getPort()));

    for (const auto& [key, value] : req.getHeaders()) {
        std::string envKey = "HTTP_" + toUpper(key);
        std::replace(envKey.begin(), envKey.end(), '-', '_');
        set(envKey, value);
    }

    return env;
}

// Helper to convert vector<string> → vector<char*>
std::vector<char*> toCharPtrArray(const std::vector<std::string>& vs) {
    std::vector<char*> out;
    out.reserve(vs.size() + 1);
    for (const auto& s : vs)
        out.push_back(const_cast<char*>(s.c_str()));
    out.push_back(nullptr);
    return out;
}

} // namespace

namespace CGI {

bool initCgiProcess(CgiProcess& cgi, const HttpRequest& req, const Server& server,
                    const Location& loc) {
    // 1) Resolve script and ensure it exists + is executable
    cgi.script_path = std::filesystem::absolute(loc.resolveAbsolutePath(req.getPath()));
    if (!isFile(cgi.script_path) || access(cgi.script_path.c_str(), X_OK) != 0) {
        return false;
    }

    // 2) Create non-blocking pipes for stdin/stdout
    int in_pipe[2], out_pipe[2];
    if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0)
        return false;
    for (int fd : {in_pipe[0], in_pipe[1], out_pipe[0], out_pipe[1]})
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

    // 3) Fork
    pid_t pid = fork();
    if (pid < 0)
        return false;

    if (pid == 0) {
        // ─── CHILD ───────────────────────────────────────────────────────────
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[1]);
        close(out_pipe[0]);

        // Determine interpreter (if any)
        std::string ext    = std::filesystem::path(cgi.script_path).extension().string();
        std::string interp = loc.getCgiInterpreter(ext);

        // Build argv: [interp?, SCRIPT_URI]
        std::vector<std::string> argvStorage;
        if (!interp.empty())
            argvStorage.push_back(interp);

        // SCRIPT_URI is the URL path used by the client
        std::string locationPath = normalizePath(loc.getPath());
        std::string scriptName   = std::filesystem::path(cgi.script_path).filename().string();
        std::string scriptUri    = locationPath;
        if (!scriptUri.empty() && scriptUri.back() != '/')
            scriptUri += '/';
        scriptUri += scriptName;

        argvStorage.push_back(scriptUri);
        auto argv = toCharPtrArray(argvStorage);

        // Build envp
        auto envStrs = prepareEnv(req, server, loc, cgi.script_path);
        auto envp    = toCharPtrArray(envStrs);

        // chdir into the script’s directory
        std::string cgiDir = std::filesystem::path(cgi.script_path).parent_path().string();
        if (chdir(cgiDir.c_str()) != 0)
            exit(1);

        // Debug: Dump argv[]
        for (size_t i = 0; argv[i] != nullptr; ++i) {
            std::cerr << "[CGI-DEBUG] argv[" << i << "] = " << argv[i] << "\n";
        }

        // Debug: Dump envp[]
        for (size_t i = 0; envp[i] != nullptr; ++i) {
            std::cerr << "[CGI-DEBUG] envp[" << i << "] = " << envp[i] << "\n";
        }

        // Exec
        execve(argv[0], argv.data(), envp.data());
        _exit(1);
    }

    // ─── PARENT ────────────────────────────────────────────────────────────
    close(in_pipe[0]);
    close(out_pipe[1]);
    cgi.pid           = pid;
    cgi.stdin_fd      = in_pipe[1];
    cgi.stdout_fd     = out_pipe[0];
    cgi.input         = (req.getMethod() == "POST" ? req.getBody() : "");
    cgi.input_sent    = 0;
    cgi.phase         = cgi.input.empty() ? CgiProcess::Phase::Reading : CgiProcess::Phase::Writing;
    cgi.start_time    = time(nullptr);
    cgi.last_activity = time(nullptr);
    return true;
}

bool handleWrite(CgiProcess& cgi) {
    const char* data = cgi.input.data() + cgi.input_sent;
    size_t      len  = cgi.input.size() - cgi.input_sent;
    ssize_t     n    = write(cgi.stdin_fd, data, len);
    if (n < 0) {
        return false;
    }
    cgi.input_sent += n;
    cgi.last_activity = time(NULL);
    if (cgi.input_sent == cgi.input.size()) {
        close(cgi.stdin_fd);
        cgi.phase = CgiProcess::Phase::Reading;
    }
    return true;
}

bool handleRead(CgiProcess& cgi) {
    char    buf[4096];
    ssize_t n = read(cgi.stdout_fd, buf, sizeof(buf));
    if (n < 0) {
        return false;
    }
    if (n == 0) {
        cgi.phase = CgiProcess::Phase::Done;
        return true;
    }
    cgi.output.append(buf, n);
    cgi.last_activity = time(NULL);
    return true;
}

std::optional<HttpResponse> finalizeCgi(CgiProcess& cgi, const Server& server,
                                        const HttpRequest& req) {
    int status;
    if (waitpid(cgi.pid, &status, WNOHANG) == 0)
        return std::nullopt; // Still running

    if (!WIFEXITED(status)) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Script terminated abnormally");
        return ResponseBuilder::generateError(500, server, req);
    }

    Logger::logFrom(LogLevel::DEBUG, "CGI", "Raw waitpid status: " + std::to_string(status));

    int exitCode = WEXITSTATUS(status);
    Logger::logFrom(LogLevel::DEBUG, "CGI", "Script exited with code: " + std::to_string(exitCode));
    if (exitCode != 0)
        return ResponseBuilder::generateError(500, server, req);

    Logger::logFrom(LogLevel::DEBUG, "CGI", "Raw waitpid status: " + std::to_string(status));

    if (cgi.output.empty()) {
        Logger::logFrom(LogLevel::WARN, "CGI", "Script produced no output");
        return ResponseBuilder::generateError(500, server, req);
    }

    size_t pos = cgi.output.find("\r\n\r\n");
    if (pos == std::string::npos) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Missing header/body delimiter");
        return ResponseBuilder::generateError(500, server, req);
    }

    std::string header = cgi.output.substr(0, pos);
    std::string body   = cgi.output.substr(pos + 4);

    std::string        contentType;
    int                statusCode     = 200;
    bool               hasContentType = false;
    std::istringstream iss(header);
    std::string        line;

    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.find("Content-Type:") == 0) {
            contentType    = trim(line.substr(13));
            hasContentType = true;
        } else if (line.find("Status:") == 0) {
            std::string statusStr = trim(line.substr(7));
            try {
                statusCode = std::stoi(statusStr);
            } catch (...) {
                Logger::logFrom(LogLevel::ERROR, "CGI", "Invalid Status header: " + statusStr);
                return ResponseBuilder::generateError(500, server, req);
            }
        }
    }

    if (!hasContentType) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Missing Content-Type header");
        return ResponseBuilder::generateError(500, server, req);
    }

    Logger::logFrom(LogLevel::DEBUG, "CGI",
                    "Parsed response: " + std::to_string(statusCode) +
                        ", content-type: " + contentType);

    return ResponseBuilder::generateSuccess(statusCode, body, contentType, req);
}

void cleanupCgi(CgiProcess& cgi) {
    if (cgi.stdin_fd > 0)
        close(cgi.stdin_fd);
    if (cgi.stdout_fd > 0)
        close(cgi.stdout_fd);
    kill(cgi.pid, SIGKILL);
    waitpid(cgi.pid, nullptr, 0);
}

bool tryTerminateCgi(CgiProcess& cgi) {
    int   status;
    pid_t result = waitpid(cgi.pid, &status, WNOHANG);
    Logger::logFrom(LogLevel::DEBUG, "CGI",
                    "tryTerminateCgi() → waitpid returned " + std::to_string(result));

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
