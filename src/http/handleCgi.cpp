/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleCgi.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/24 12:23:37 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/31 16:37:51 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handleCgi.hpp"
#include "http/HttpResponseBuilder.hpp"
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

    // PATH_INFO = remainder of the path after SCRIPT_NAME
    std::string pathInfo;
    if (requestPath.rfind(scriptUri, 0) == 0 && requestPath.size() > scriptUri.size()) {
        pathInfo = requestPath.substr(scriptUri.size());
        if (!pathInfo.empty() && pathInfo.front() != '/')
            pathInfo.insert(pathInfo.begin(), '/');
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
                    const Location& loc) {
    cgi.last_activity = time(NULL);
    cgi.script_path   = std::filesystem::absolute(loc.resolveAbsolutePath(req.getPath()));
    Logger::logFrom(LogLevel::DEBUG, "CGI", "Initializing CGI for script: " + cgi.script_path);
    if (!isFile(cgi.script_path)) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "File is invalid");
        return false;
    }
    if (access(cgi.script_path.c_str(), X_OK) != 0) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "File is not executable");
        return false;
    }

    std::cerr << "[CGI] Script is executable: " << cgi.script_path << std::endl;

    // === Generate a unique temporary file path ===
    static int        counter = 0;
    std::stringstream ss;
    ss << "/home/toonsa/myProjects/webserv/temp_in" << getpid() << "_" << time(nullptr) << "_"
       << counter++ << ".tmp";
    std::string       temp_in = ss.str();
    std::stringstream ss1;
    ss1 << "/home/toonsa/myProjects/webserv/temp_out_" << getpid() << "_" << time(nullptr) << "_"
        << counter++ << ".tmp";
    std::string temp_out = ss1.str();
    cgi.input_path       = temp_in;
    cgi.output_path      = temp_out;

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

    if (pid > 0) {
        Logger::logFrom(LogLevel::DEBUG, "CGI",
                        "Forked PID: " + std::to_string(pid) + ", script: " + cgi.script_path);
    }

    if (pid == 0) {
        dup2(body_fd, STDIN_FILENO);
        close(body_fd);
        dup2(output_fd, STDOUT_FILENO);
        close(output_fd);

        // 1. Store script and interpreter in scoped std::string
        std::string scriptPath = cgi.script_path;
        std::string ext        = std::filesystem::path(scriptPath).extension().string();
        std::string interp     = loc.getCgiInterpreter(ext);

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
    output_fd = open(temp_out.c_str(), O_RDONLY);
    if (output_fd < 0) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Failed to reopen CGI temp_out file for reading");
        return false;
    }

    cgi.pid           = pid;
    cgi.stdout_fd     = output_fd;
    cgi.phase         = CgiProcess::Phase::Reading;
    cgi.start_time    = time(NULL);
    cgi.last_activity = time(NULL);
    return true;
}

std::optional<HttpResponse> finalizeCgi(CgiProcess& cgi, const Server& server,
                                        const HttpRequest& req) {
    // Logger::logFrom(LogLevel::DEBUG, "CGI finalizeCgi", "Finalizing CGI process for script");
    int status;
    if (waitpid(cgi.pid, &status, WNOHANG) == 0) {
        // Logger::logFrom(LogLevel::DEBUG, "CGI finalizeCgi", "CGI process is still running");
        return std::nullopt; // Not done yet
    }
    /* if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "finalizeCgi(): CGI process exited with error: " +
    std::to_string(WEXITSTATUS(status))); return ResponseBuilder::generateError(502, server, req);
    } */

    cgi.last_activity = time(NULL);
    std::ifstream in(cgi.output_path);
    if (!in.is_open()) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Failed to open CGI output file");
        return ResponseBuilder::generateError(500, server, req);
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string fullOutput = buffer.str();
    in.close();

    // Basic parser (header + body)
    size_t pos = fullOutput.find("\r\n\r\n");
    Logger::logFrom(LogLevel::DEBUG, "CGI", "cgi is checking output for header");
    std::string header;
    std::string body;
    if (pos != std::string::npos) {
        header = fullOutput.substr(0, pos);
        body   = fullOutput.substr(pos + 4);
    } else {
        pos = fullOutput.find("\n\n");
        if (pos != std::string::npos) {
            header = fullOutput.substr(0, pos);
            body   = fullOutput.substr(pos + 2);
        }
    }
    if (pos == std::string::npos) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "finalizeCgi(): no header found in output");
        return ResponseBuilder::generateError(500, server, req);
    }
    std::string        contentType = "text/plain";
    int                code        = 200;
    std::istringstream iss(header);
    std::string        line;

    while (std::getline(iss, line)) {
        if (line.find("Content-Type:") == 0)
            contentType = trim(line.substr(13));
        else if (line.find("Status:") == 0)
            code = std::stoi(trim(line.substr(7)));
    }
    return ResponseBuilder::generateSuccess(code, body, contentType, req);
}

void cleanupCgi(CgiProcess& cgi) {
    if (cgi.stdout_fd > 0)
        close(cgi.stdout_fd);
    kill(cgi.pid, SIGKILL);
    waitpid(cgi.pid, nullptr, 0);
    if (!cgi.input_path.empty()) {
        unlink(cgi.input_path.c_str());
        Logger::logFrom(LogLevel::DEBUG, "CGI", "Deleted input temp file: " + cgi.input_path);
    }

    if (!cgi.output_path.empty()) {
        unlink(cgi.output_path.c_str());
        Logger::logFrom(LogLevel::DEBUG, "CGI", "Deleted output temp file: " + cgi.output_path);
    }
}

bool tryTerminateCgi(CgiProcess& cgi) {
    int   status;
    pid_t result = waitpid(cgi.pid, &status, WNOHANG);
    // Logger::logFrom(LogLevel::DEBUG, "CGI", "tryTerminateCgi() → waitpid returned " +
    // std::to_string(result));

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
