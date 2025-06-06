/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleCgi.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/24 12:23:37 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/05 11:07:07 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handleCgi.hpp"
#include "http/responseBuilder.hpp"
#include "network/SocketManager.hpp"
#include "utils/Logger.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/stringUtils.hpp"

#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
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

    std::string pathInfo;
    if (requestPath.size() > scriptUri.size() &&
        requestPath.compare(0, scriptUri.size(), scriptUri) == 0) {
        pathInfo = requestPath.substr(scriptUri.size());
        if (!pathInfo.empty() && pathInfo[0] != '/')
            pathInfo = "/" + pathInfo;
    }
    set("SCRIPT_NAME", req.getPath());
    if (pathInfo.empty()) {
        set("PATH_INFO", req.getPath());
    } else {
        set("PATH_INFO", pathInfo);
    }
    set("REQUEST_METHOD", req.getMethod());
    set("QUERY_STRING", req.getQuery());
    set("CONTENT_LENGTH", std::to_string(req.getContentLength()));
    if (!req.getHeader("Content-Type").empty())
        set("CONTENT_TYPE", req.getHeader("Content-Type"));

    set("SERVER_PROTOCOL", "HTTP/1.1");
    set("GATEWAY_INTERFACE", "CGI/1.1");
    set("SERVER_SOFTWARE", "webserv/1.0");
    set("DOCUMENT_ROOT", loc.getRoot());
    set("SERVER_NAME", server.getDefaultServerName());
    set("SERVER_PORT", std::to_string(server.getPort()));
    set("PATH_TRANSLATED", scriptPath);
    set("REMOTE_ADDR", "127.0.0.1");
    set("REQUEST_URI", req.getPath());
    set("SCRIPT_FILENAME", scriptPath);
    set("REDIRECT_STATUS", "200");

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
                    const Location& loc, const std::vector<pollfd>& poll_fds) {
    cgi.last_activity = time(NULL);
    cgi.script_path   = std::filesystem::absolute(loc.resolveAbsolutePath(req.getPath()));
    if (!isFile(cgi.script_path)) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "File is invalid");
        return false;
    }
    if (access(cgi.script_path.c_str(), X_OK) != 0) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "File is not executable");
        return false;
    }

    // === Generate a unique temporary file path ===
    static unsigned counter  = 0;
    std::string     temp_in  = make_temp_name("webserv_in", counter);
    std::string     temp_out = make_temp_name("webserv_out", counter);
    cgi.input_path           = temp_in;
    cgi.output_path          = temp_out;

    // === Write request body to temp file ===
    std::ofstream out(temp_in, std::ios::binary);
    if (!out.is_open()) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Failed to create temp file");
        return false;
    }
    out.write(req.getBody().data(), req.getBody().size());
    out.close();

    // === Open the temp file for reading ===
    int body_fd = open(temp_in.c_str(), O_RDONLY);
    if (body_fd < 0) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Failed to reopen temp_in file for CGI input");
        return false;
    }
    cgi.last_activity = time(NULL);
    int output_fd     = open(temp_out.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (output_fd < 0) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Failed to create CGI output file");
        close(body_fd);
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(body_fd);
        close(output_fd);
        return false;
    }

    if (pid == 0) {
        dup2(body_fd, STDIN_FILENO);
        close(body_fd);
        dup2(output_fd, STDOUT_FILENO);
        close(output_fd);
        for (std::vector<pollfd>::const_iterator it = poll_fds.begin(); it != poll_fds.end();
             ++it) {
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
        Logger::logFrom(LogLevel::ERROR, "CGI CHILD",
                        "execve failed: " + std::string(strerror(errno)));
        exit(1);
    }

    close(body_fd);
    close(output_fd);

    cgi.pid           = pid;
    cgi.start_time    = time(NULL);
    cgi.last_activity = time(NULL);
    return true;
}

HttpResponse finalizeCgi(CgiProcess& cgi, const Server& server, const HttpRequest& req) {

    cgi.last_activity = time(NULL);
    std::ifstream in(cgi.output_path, std::ios::binary);
    if (!in.is_open()) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Failed to open CGI output file");
        return ResponseBuilder::generateError(500, server, req);
    }

    in.seekg(0, std::ios::end);
    std::streamsize totalSize = in.tellg();
    in.seekg(0, std::ios::beg);

    // Read the first 16KB only to find headers
    const size_t      MAX_HEADER_SCAN = 16 * 1024;
    std::vector<char> buffer(MAX_HEADER_SCAN);
    in.read(buffer.data(), MAX_HEADER_SCAN);
    std::streamsize bytesRead = in.gcount();
    std::string     partialOutput(buffer.data(), bytesRead);

    // Look for header delimiter
    size_t pos      = partialOutput.find("\r\n\r\n");
    size_t delimLen = 4;
    if (pos == std::string::npos) {
        pos      = partialOutput.find("\n\n");
        delimLen = 2;
    }
    if (pos == std::string::npos) {
        Logger::logFrom(LogLevel::ERROR, "CGI",
                        "finalizeCgi(): Header delimiter not found in first 16KB");
        return ResponseBuilder::generateError(500, server, req);
    }

    std::string header      = partialOutput.substr(0, pos);
    std::string contentType = "text/plain";
    int         code        = 200;

    std::istringstream headerStream(header);
    std::string        line;
    while (std::getline(headerStream, line)) {
        if (line.find("Content-Type:") == 0)
            contentType = trim(line.substr(13));
        else if (line.find("Status:") == 0)
            code = std::stoi(trim(line.substr(7)));
    }

    std::streamsize headerEnd = static_cast<std::streamsize>(pos + delimLen);
    std::streamsize bodySize  = totalSize - headerEnd;
    in.close();

    HttpResponse resp = ResponseBuilder::generateSuccessFile(code, cgi.output_path, contentType,
                                                             req, bodySize, headerEnd);
    resp.setCgiTempFile(cgi.output_path);
    Logger::logFrom(LogLevel::kDEBUG, "CGI",
                    "CGI process completed with PID: " + std::to_string(cgi.pid) +
                        ", output file: " + cgi.output_path +
                        ", status code: " + std::to_string(code));

    return resp;
}

void errorOnCgi(CgiProcess& cgi) {
    Logger::logFrom(LogLevel::kDEBUG, "CGI",
                    "Killing CGI process with PID: " + std::to_string(cgi.pid));
    kill(cgi.pid, SIGKILL);
    waitpid(cgi.pid, nullptr, 0);
    if (!cgi.input_path.empty()) {
        if (unlink(cgi.input_path.c_str()) == 0) {
            Logger::logFrom(LogLevel::kDEBUG, "CGI", "Deleted input temp file: " + cgi.input_path);
        } else {
            Logger::logFrom(LogLevel::ERROR, "CGI",
                            "Failed to delete input temp file: " + cgi.input_path);
        }
    }
    if (!cgi.output_path.empty()) {
        if (unlink(cgi.output_path.c_str()) == 0) {
            Logger::logFrom(LogLevel::kDEBUG, "CGI",
                            "Deleted output temp file: " + cgi.output_path);
        } else {
            Logger::logFrom(LogLevel::ERROR, "CGI",
                            "Failed to delete output temp file: " + cgi.output_path);
        }
    }

    cgi.pid           = -1;
    cgi.start_time    = 0;
    cgi.last_activity = 0;
    cgi.input_path.clear();
    cgi.script_path.clear();
}

void cleanupCgi(CgiProcess& cgi) {
    // Only delete input file (output file is managed by HttpResponse)
    if (!cgi.input_path.empty()) {
        if (unlink(cgi.input_path.c_str()) == 0) {
            Logger::logFrom(LogLevel::kDEBUG, "CGI", "Deleted input temp file: " + cgi.input_path);
        } else {
            Logger::logFrom(LogLevel::ERROR, "CGI",
                            "Failed to delete input temp file: " + cgi.input_path);
        }
    }
    cgi.pid           = -1;
    cgi.start_time    = 0;
    cgi.last_activity = 0;
    cgi.input_path.clear();
}

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
    Logger::logFrom(LogLevel::kDEBUG, "CGI",
                    "CGI process terminated with PID: " + std::to_string(cgi.pid));
    return true;
}

} // namespace CGI
