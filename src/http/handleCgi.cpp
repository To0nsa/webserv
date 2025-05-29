/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleCgi.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/24 12:23:37 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/29 16:01:13 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handleCgi.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/stringUtils.hpp"
#include "utils/Logger.hpp"
#include <cstdio>
#include <fstream>
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
    std::string scriptUri    = locationPath;
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
    /* Logger::logFrom(LogLevel::DEBUG, "CGI-ENV", "SCRIPT_NAME = " + scriptUri);
    Logger::logFrom(LogLevel::DEBUG, "CGI-ENV", "PATH_INFO = " + pathInfo);
    Logger::logFrom(LogLevel::DEBUG, "CGI-ENV", "LOCATION_PATH = " + locationPath); */
    set("SCRIPT_NAME", scriptUri);
    /* if (!pathInfo.empty()) */
    set("PATH_INFO", pathInfo);
    /* set("SCRIPT_NAME", req.getPath());
    set("PATH_INFO", scriptPath); */

    set("REQUEST_METHOD", req.getMethod());
	/* if (!req.getQuery().empty()) */
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

std::vector<char*> toCharPtrArray(const std::vector<std::string>& vec) {
    std::vector<char*> out;
    for (const auto& s : vec)
        out.push_back(const_cast<char*>(s.c_str()));
    out.push_back(nullptr);
    return out;
}

} // namespace

namespace CGI {

bool initCgiProcess(CgiProcess& cgi, const HttpRequest& req, const Server& server,
                    const Location& loc) {
    cgi.script_path = std::filesystem::absolute(loc.resolveAbsolutePath(req.getPath()));
	Logger::logFrom(LogLevel::DEBUG, "CGI", "Initializing CGI for script: " + cgi.script_path);
    if (!isFile(cgi.script_path))
        return false;
    if (access(cgi.script_path.c_str(), X_OK) != 0) {
        return false;
	}

	std::cerr << "[CGI] Script is executable: " << cgi.script_path << std::endl;

    // === Generate a unique temporary file path ===
    static int counter = 0;
    std::stringstream ss;
    ss << "/tmp/webserv_tmpfile_" << getpid() << "_" << time(nullptr) << "_" << counter++ << ".tmp";
    std::string temp_path = ss.str();

    // === Write request body to temp file ===
    std::ofstream out(temp_path, std::ios::binary);
    if (!out.is_open()) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Failed to create temp file");
        return false;
    }
    out.write(req.getBody().data(), req.getBody().size());
    out.close();

    // === Open the temp file for reading ===
    int body_fd = open(temp_path.c_str(), O_RDONLY);
    if (body_fd < 0) {
        Logger::logFrom(LogLevel::ERROR, "CGI", "Failed to reopen temp file for CGI input");
        return false;
    }

    std::filesystem::remove(temp_path); // auto-delete after fd close

    // === Create stdout pipe ===
    int out_pipe[2];
    if (pipe(out_pipe) < 0) {
        close(body_fd);
        return false;
    }

    fcntl(out_pipe[0], F_SETFL, fcntl(out_pipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(out_pipe[1], F_SETFL, fcntl(out_pipe[1], F_GETFL) | O_NONBLOCK);

    pid_t pid = fork();
    if (pid < 0) {
        close(body_fd);
        close(out_pipe[0]);
        close(out_pipe[1]);
        return false;
    }

    if (pid > 0) {
        Logger::logFrom(LogLevel::DEBUG, "CGI", "Forked PID: " + std::to_string(pid) + ", script: " + cgi.script_path);
    }

    if (pid == 0) {
        dup2(body_fd, STDIN_FILENO);
        close(body_fd);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(out_pipe[1]);
        close(out_pipe[0]);

        // 1. Store script and interpreter in scoped std::string
        std::string scriptPath = cgi.script_path;
        std::string ext        = std::filesystem::path(scriptPath).extension().string();
        std::string interp     = loc.getCgiInterpreter(ext);
        //Logger::logFrom(LogLevel::DEBUG, "CGI CHILD", "Interpreter: " + interp + ", Script: " + scriptPath);

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
        //Logger::logFrom(LogLevel::DEBUG, "CGI CHILD", "Changing directory to: " + cgiDir);
        if (chdir(cgiDir.c_str()) != 0) {
            //Logger::logFrom(LogLevel::ERROR, "CGI CHILD", "chdir failed: " + std::string(strerror(errno)));
            exit(1);
        }
		
        // 5. Final call
        /* std::cout << "[CGI CHILD] Executing script: " << scriptPath << std::endl;
        for (size_t i = 0; argv[i]; ++i)
			Logger::logFrom(LogLevel::DEBUG, "CGI CHILD-ARGV[" + std::to_string(i) + "]", argv[i]);

		for (size_t i = 0; envp[i]; ++i)
			Logger::logFrom(LogLevel::DEBUG, "CGI CHILD-ENV[" + std::to_string(i) + "]", envp[i]); */
        execve(argv[0], argv.data(), envp.data());

        // 6. If execve fails
        Logger::logFrom(LogLevel::ERROR, "CGI CHILD", "execve failed: " + std::string(strerror(errno)));
        exit(1);
    }

    close(body_fd);
    close(out_pipe[1]);

    cgi.pid           = pid;
    cgi.stdout_fd     = out_pipe[0];
    cgi.phase         = CgiProcess::Phase::Reading;
    cgi.start_time    = time(NULL);
    cgi.last_activity = time(NULL);
    return true;
}

bool handleRead(CgiProcess& cgi) {
	Logger::logFrom(LogLevel::DEBUG, "CGI HANDLE READ", "Handling read phase for CGI process");
    char    buf[4096];
    ssize_t n = read(cgi.stdout_fd, buf, sizeof(buf));
    if (n < 0) {
        return false;
    }
    if (n == 0) {
		Logger::logFrom(LogLevel::DEBUG, "CGI READ", "EOF reached");
        Logger::logFrom(LogLevel::DEBUG, "CGI READ", "CGI process output: [[[[[" + cgi.output + "]]]]]");
        cgi.phase = CgiProcess::Phase::Done;
        return true;
    }
    cgi.output.append(buf, n);
    cgi.last_activity = time(NULL);
    return true;
}

std::optional<HttpResponse> finalizeCgi(CgiProcess& cgi, const Server& server,
                                        const HttpRequest& req) {
	Logger::logFrom(LogLevel::DEBUG, "CGI finalizeCgi", "Finalizing CGI process for script");
    int status;
    if (waitpid(cgi.pid, &status, WNOHANG) == 0) {
        Logger::logFrom(LogLevel::DEBUG, "CGI finalizeCgi", "CGI process is still running");
        return std::nullopt; // Not done yet
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		Logger::logFrom(LogLevel::ERROR, "CGI", "finalizeCgi(): CGI process exited with error: " + std::to_string(WEXITSTATUS(status)));
        return ResponseBuilder::generateError(502, server, req);
	}

    // Basic parser (header + body)
    size_t pos = cgi.output.find("\r\n\r\n");
	Logger::logFrom(LogLevel::DEBUG, "CGI", "cgi is checking output for header");
    if (pos == std::string::npos) {
		Logger::logFrom(LogLevel::ERROR, "CGI", "finalizeCgi(): no header found in output");
        return ResponseBuilder::generateError(500, server, req);
		//return ResponseBuilder::generateSuccess(200, cgi.output, "", req);
	}

    std::string header = cgi.output.substr(0, pos);
    std::string body   = cgi.output.substr(pos + 4);

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

    // DEBUG
    Logger::logFrom(LogLevel::DEBUG, "CGI", "finalizeCgi(): output =\n" + cgi.output);

    return ResponseBuilder::generateSuccess(code, body, contentType, req);
}

void cleanupCgi(CgiProcess& cgi) {
    if (cgi.stdout_fd > 0)
        close(cgi.stdout_fd);
    kill(cgi.pid, SIGKILL);
    waitpid(cgi.pid, nullptr, 0);
}

bool tryTerminateCgi(CgiProcess& cgi) {
    int   status;
    pid_t result = waitpid(cgi.pid, &status, WNOHANG);
    Logger::logFrom(LogLevel::DEBUG, "CGI", "tryTerminateCgi() → waitpid returned " + std::to_string(result));

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
