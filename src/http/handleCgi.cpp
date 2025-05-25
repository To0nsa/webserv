/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleCgi.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/24 12:23:37 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/24 20:05:55 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handleCgi.hpp"
#include "http/HttpResponseBuilder.hpp"
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

    set("REQUEST_METHOD", req.getMethod());
    set("SCRIPT_NAME", req.getPath());
    set("PATH_INFO", scriptPath);
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
    if (!isFile(cgi.script_path))
        return false;
    if (access(cgi.script_path.c_str(), X_OK) != 0)
        return false;

    int in_pipe[2], out_pipe[2];
    if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0) {
        return false;
    }

    fcntl(in_pipe[0], F_SETFL, fcntl(in_pipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(in_pipe[1], F_SETFL, fcntl(in_pipe[1], F_GETFL) | O_NONBLOCK);
    fcntl(out_pipe[0], F_SETFL, fcntl(out_pipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(out_pipe[1], F_SETFL, fcntl(out_pipe[1], F_GETFL) | O_NONBLOCK);

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }

    if (pid > 0) {
        std::cerr << "[CGI] Forked PID: " << pid << ", script: " << cgi.script_path << std::endl;
    }

    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[1]);
        close(out_pipe[0]);

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
            perror("chdir");
            exit(1);
        }

        // 5. Final call
        // DEBUG
        std::cerr << "[CGI] execve: " << argv[0] << std::endl;
        execve(argv[0], argv.data(), envp.data());

        // 6. If execve fails
        perror("execve");
        exit(1);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);

    cgi.pid           = pid;
    cgi.stdin_fd      = in_pipe[1];
    cgi.stdout_fd     = out_pipe[0];
    cgi.input         = req.getMethod() == "POST" ? req.getBody() : "";
    cgi.input_sent    = 0;
    cgi.phase         = cgi.input.empty() ? CgiProcess::Phase::Reading : CgiProcess::Phase::Writing;
    cgi.start_time    = time(NULL);
    cgi.last_activity = time(NULL);
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
        return std::nullopt; // Not done yet
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return ResponseBuilder::generateError(502, server, req);

    // Basic parser (header + body)
    size_t pos = cgi.output.find("\r\n\r\n");
    if (pos == std::string::npos)
        return ResponseBuilder::generateError(500, server, req);

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
    std::cerr << "[CGI] finalizeCgi(): output =\n" << cgi.output << "\n";

    return ResponseBuilder::generateSuccess(code, body, contentType, req);
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
    std::cerr << "[CGI] tryTerminateCgi() → waitpid returned " << result << "\n";

    if (result == 0) {
        return false;
    }

    if (result == -1) {
        perror("waitpid");
        return true;
    }
    return true;
}

} // namespace CGI
