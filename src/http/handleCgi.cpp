/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleCgi.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/24 12:23:37 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/17 12:19:24 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

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

std::pair<std::string, std::streamsize> readInitialOutput(std::ifstream& file, size_t maxBytes) {
    std::vector<char> buffer(maxBytes);
    file.read(buffer.data(), maxBytes);
    std::streamsize bytesRead = file.gcount();
    return {std::string(buffer.data(), bytesRead), bytesRead};
}

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

void cleanupCgi(CgiProcess& cgi) {
    unlinkWithErrorLog(cgi.input_path, "input temp file");
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
    return true;
}

} // namespace CGI
