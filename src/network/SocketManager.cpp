/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:51:20 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/05 16:17:31 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "network/SocketManager.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpRequestHandler.hpp"
#include "http/HttpRequestParser.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "utils/Logger.hpp"
#include "utils/filesystemUtils.hpp"
#include <sstream> // For stringstream, we will remove it later

// Signal handler for exiting the server
static volatile sig_atomic_t running = 1;

// We don't need it. We can handle it directly in poll. Let's discuss.
static void signalHandler(int signum) {
    if (signum == SIGINT)
        running = 0;
}

// Constructor: sets up sockets for each server defined in the config
SocketManager::SocketManager(const std::vector<Server>& servers) {
    signal(SIGINT, signalHandler);
    signal(SIGPIPE, SIG_IGN);
    setupSockets(servers);
}

SocketManager::~SocketManager() {
    for (const pollfd& pfd : _poll_fds)
        close(pfd.fd);
}

void SocketManager::cleanupCgiForClient(int client_fd) {
    if (!_client_info.contains(client_fd))
        return;

    ClientInfo& client = _client_info[client_fd];
    if (!client.cgiProcess)
        return;

    CGI::cleanupCgi(*client.cgiProcess);
    client.cgiProcess.reset();
    client.isCgiProcessRunning = false;
    client.currentCgiRequest   = HttpRequest();
}

void SocketManager::cleanupClientConnectionClose(int client_fd, size_t index) {
    // 1. Defensive: bounds check for poll index
    if (index < _poll_fds.size()) {
        _poll_fds.erase(_poll_fds.begin() + index);
    }

    // 2. Cleanup CGI and file_stream if client exists
    auto it = _client_info.find(client_fd);
    if (it != _client_info.end()) {
        ClientInfo& client = it->second;

        // Clean up any pending responses with temporary files
        while (!client.responses.empty()) {
            HttpResponse& resp = client.responses.front();

            // Delete temporary files from CGI responses
            if (resp.isCgiTempFile()) {
                CGI::unlinkWithErrorLog(resp.getCgiTempFile(), "out temp file");
            }
            client.responses.pop();
        }

        // Clean up CGI process
        if (client.cgiProcess) {
            CGI::errorOnCgi(*client.cgiProcess);
            client.cgiProcess.reset();
            client.isCgiProcessRunning = false;
            client.currentCgiRequest   = HttpRequest();
        }

        // Close file stream
        if (client.file_stream.is_open()) {
            client.file_stream.close();
        }

        _client_info.erase(it);
    }

    // Close socket
    close(client_fd);

    Logger::logFrom(LogLevel::INFO, "SocketManager cleanupClientConnectionClose",
                    "Closed FD (Connection: close): " + std::to_string(client_fd));
}

void SocketManager::resetRequestState(int client_fd) {
    if (!_client_info.count(client_fd))
        return;
    // If there is still any data in requestBuffer, treat it as a partial header:
    if (!_client_info[client_fd].requestBuffer.empty()) {
        _client_info[client_fd].headerComplete = false;
        // headerBytesReceived should reflect how many bytes are already in the buffer.
        // But if we are just about to parse a brand‐new header, headerBytesReceived
        // should have already been set by receiveFromClient(...) when those bytes first arrived.
        // So here we do NOT zero it out—leave it alone so the header‐timer can still tick.
        return;
    }
    // If requestBuffer is empty, then there is no partial header in progress.
    _client_info[client_fd].headerComplete      = false;
    _client_info[client_fd].headerBytesReceived = 0;
    _client_info[client_fd].bodyBytesReceived   = 0;
}

bool SocketManager::isHeaderTimeout(int fd, time_t now) {
    ClientInfo& client = _client_info[fd];

    if (client.responses.empty() && client.current_raw_response.empty() && !client.headerComplete &&
        client.headerBytesReceived > 0 &&
        now - client.connectionStartTime > HEADER_TIMEOUT_SECONDS) {
        Logger::logFrom(LogLevel::WARN, "SocketManager",
                        "Header timeout on fd: " + std::to_string(fd));
        respondError(fd, 408);
        return true;
    }

    return false;
}

bool SocketManager::isBodyTimeout(int fd, time_t now) {
    ClientInfo& client = _client_info[fd];
    if (client.headerComplete && now - client.connectionStartTime > TIMEOUT) {
        Logger::logFrom(LogLevel::WARN, "SocketManager",
                        "Body timeout on fd: " + std::to_string(fd));
        respondError(fd, 408);
        return true;
    }
    return false;
}

bool SocketManager::isSendTimeout(int fd, time_t now) {
    ClientInfo& client = _client_info[fd];
    if (!client.responses.empty() && !client.current_raw_response.empty() &&
        now - client.lastSendAttemptTime > TIMEOUT) {
        Logger::logFrom(LogLevel::WARN, "SocketManager",
                        "Send timeout on fd: " + std::to_string(fd));
        return true;
    }
    return false;
}

bool SocketManager::isIdleTimeout(int fd, time_t now) {
    ClientInfo& client = _client_info[fd];
    if (client.cgiProcess.has_value()) {
        return false;
    }
    if (client.responses.empty() && client.current_raw_response.empty() && !client.headerComplete &&
        client.headerBytesReceived == 0 && now - client.lastRequestTime > TIMEOUT) {
        Logger::logFrom(LogLevel::WARN, "SocketManager",
                        "Idle timeout on fd: " + std::to_string(fd));
        return true;
    }
    return false;
}

bool SocketManager::checkClientTimeouts(int client_fd, size_t index) {
    if (!_client_info.count(client_fd))
        return false;

    time_t now = time(NULL);
    if (isIdleTimeout(client_fd, now) || isSendTimeout(client_fd, now)) {
        cleanupClientConnectionClose(client_fd, index);
        return false;
    }
    if (isHeaderTimeout(client_fd, now) || isBodyTimeout(client_fd, now)) {
        _poll_fds[index].events &= ~POLLIN;
        return true;
    }
    return false;
}

void SocketManager::handlePollError(int fd, size_t index, short revents) {
    if (revents & POLLNVAL) {
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "Invalid poll event on fd: " + std::to_string(fd));
    } else if (revents & POLLERR) {
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "Socket error on fd: " + std::to_string(fd));
    } else if (revents & POLLHUP) {
        Logger::logFrom(LogLevel::INFO, "SocketManager",
                        "Client disconnected (POLLHUP) on fd: " + std::to_string(fd));
    }
    cleanupClientConnectionClose(fd, index);
}

bool SocketManager::receiveFromClient(int client_fd, size_t index) {
    char buffer[RECV_BUFFER];
    _client_info[client_fd].lastRequestTime = time(NULL);
    int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0); // MacOS only
    // int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
    if (bytes == 0) {
        Logger::logFrom(LogLevel::INFO, "SocketManager",
                        "Client fd " + std::to_string(client_fd) + " disconnected.");
        cleanupClientConnectionClose(client_fd, index);
        return false;
    }
    if (bytes < 0) {
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        std::string("recv() failed: ") + std::strerror(errno));
        cleanupClientConnectionClose(client_fd, index);
        return false;
    }
    buffer[bytes] = '\0';

    std::string single_msg(buffer, bytes);
    _client_info[client_fd].requestBuffer += single_msg;
    size_t headerEndPos = _client_info[client_fd].requestBuffer.find("\r\n\r\n");

    if (headerEndPos == std::string::npos) {
        if (_client_info[client_fd].headerBytesReceived == 0) {
            _client_info[client_fd].connectionStartTime = time(NULL);
        }
        _client_info[client_fd].headerBytesReceived += bytes;
    } else {
        if (!_client_info[client_fd].headerComplete) {
            size_t fullHeaderSize = headerEndPos + 4;
            size_t oldBufferSize  = _client_info[client_fd].requestBuffer.size() - bytes;
            size_t headerBytesThisTime =
                std::max((ssize_t) 0, (ssize_t) (fullHeaderSize - oldBufferSize));
            _client_info[client_fd].headerBytesReceived += headerBytesThisTime;
            _client_info[client_fd].bodyBytesReceived += (bytes - headerBytesThisTime);
            _client_info[client_fd].headerComplete = true;
        } else {
            // Header already counted, this must be body
            _client_info[client_fd].bodyBytesReceived += bytes;
        }
    }

    return true;
}

void SocketManager::respondError(int fd, int status_code) {
    HttpRequest  empty;
    HttpResponse err =
        ResponseBuilder::generateError(status_code, _client_info[fd].serverConfig, empty);
    _client_info[fd].responses.push(err);
}

bool SocketManager::checkRequestLimits(int fd) {
    ClientInfo& client = _client_info[fd];

    // Only enforce header-length limit while headers are still incomplete
    if (client.headerBytesReceived > HEADER_MAX_LENGTH) {
        Logger::logFrom(LogLevel::WARN, "SocketManager",
                        "Request header too large from fd: " + std::to_string(fd));
        respondError(fd, 431); // Request Header Fields Too Large
        return true;
    }

    return false;
}

// Custom exception for socket errors
SocketManager::SocketError::SocketError(const std::string& msg) {
    _msg = msg;
}

const char* SocketManager::SocketError::what() const throw() {
    return (_msg.c_str());
}

// Set up sockets for each server (host:port)
void SocketManager::setupSockets(const std::vector<Server>& servers) {
    for (size_t i = 0; i < servers.size(); ++i) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        // int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0); // Create a TCP socket
        if (fd < 0)
            throw SocketError("socket() failed: " + std::string(std::strerror(errno)));

        int opt =
            1; // To tell the OS: "I want to reuse this port immediately, even if it's in TIME_WAIT
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            close(fd);
            throw SocketError("setsockopt() failed: " + std::string(std::strerror(errno)));
        }

        // MacOS
        if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) { // Make socket non-blocking
            close(fd);
            throw SocketError("fcntl() failed: " + std::string(std::strerror(errno)));
        }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(servers[i].getPort()); // Convert port to network byte order

        // Convert hostname to IP address
        if (servers[i].getHost() == "localhost")
            addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        else
            addr.sin_addr.s_addr = inet_addr(servers[i].getHost().c_str());

        if (bind(fd, (sockaddr*) &addr, sizeof(addr)) < 0) { // Bind socket to IP:port
            close(fd);
            throw SocketError("bind() failed on " + servers[i].getHost() + ":" +
                              std::to_string(servers[i].getPort()) + ": " + strerror(errno));
        }

        if (listen(fd, SOMAXCONN) < 0) { // Start listening for incoming connections
            close(fd);
            throw SocketError("listen() failed: " + std::string(std::strerror(errno)));
        }

        // Register fd in poll list
        _poll_fds.push_back((pollfd){fd, POLLIN, 0});
        _listen_map[fd] = servers[i]; // Map fd to its corresponding server

        Logger::logFrom(LogLevel::INFO, "SocketManager",
                        "Listening on " + servers[i].getHost() + ":" +
                            std::to_string(servers[i].getPort()));
    }
}

void SocketManager::run() {
    while (running) {
        int n = poll(&_poll_fds[0], _poll_fds.size(), 1000);
        if (n < 0) {
            if (errno == EINTR) {
                running = 0;
                continue;
            }
            throw SocketError("poll() failed: " + std::string(std::strerror(errno)));
        }

        // handleCgiPollEvents();
        //  === CGI Completion Check ===
        for (auto& [client_fd, client] : _client_info) {
            if (!client.cgiProcess)
                continue;

            CgiProcess& cgi = *client.cgiProcess;

            // timeout check
            if (time(NULL) - cgi.last_activity > CGI_TIMEOUT_SECONDS) {
                Logger::logFrom(LogLevel::WARN, "CGI",
                                "Timeout. Killing CGI process for fd: " +
                                    std::to_string(client_fd));
                client.responses.push(ResponseBuilder::generateError(504, client.serverConfig, {}));
                CGI::errorOnCgi(cgi);
                client.cgiProcess.reset();
                client.isCgiProcessRunning = false;
                client.currentCgiRequest   = HttpRequest(); // clears request
                for (auto& pfd : _poll_fds) {
                    if (pfd.fd == client_fd) {
                        pfd.events |= POLLOUT;
                        break;
                    }
                }
                continue;
            }

            // check if finished
            if (CGI::tryTerminateCgi(cgi)) {
                HttpResponse resp =
                    CGI::finalizeCgi(cgi, client.serverConfig, client.currentCgiRequest);
                // HttpResponse resp = maybeResp.value_or(ResponseBuilder::generateError(502,
                // client.serverConfig, {}));
                client.responses.push(resp);
                CGI::cleanupCgi(cgi);
                client.cgiProcess.reset();
                client.isCgiProcessRunning = false;         // optional if used independently
                client.currentCgiRequest   = HttpRequest(); // clears request

                for (auto& pfd : _poll_fds) {
                    if (pfd.fd == client_fd) {
                        pfd.events |= POLLOUT;
                        break;
                    }
                }
                size_t idx = 0;
                for (; idx < _poll_fds.size(); ++idx) {
                    if (_poll_fds[idx].fd == client_fd) {
                        break;
                    }
                }
                if (idx < _poll_fds.size()) {
                    processPendingRequests(client_fd);
                }
            }
        }

        // Then handle sockets — but skip *all* CGI FDs before doing error/HUP checks
        for (size_t i = _poll_fds.size(); i-- > 0;) {
            short revents    = _poll_fds[i].revents;
            int   current_fd = _poll_fds[i].fd;
            if (checkClientTimeouts(current_fd, i)) {
                _poll_fds[i].events |= POLLOUT; // If connection keep-alive but client idle we close
            }

            // **FIX**: skip CGI pipe FDs entirely
            if (_fd_to_cgi.contains(current_fd)) {
                continue;
            }

            // Now error/hangup on *client* sockets
            if (revents & POLLERR || revents & POLLHUP || revents & POLLNVAL) {
                handlePollError(current_fd, i, revents);
                continue;
            }

            if (revents & POLLIN) {
                if (_listen_map.count(current_fd)) {
                    handleNewConnection(current_fd);
                } else {
                    if (!handleClientData(current_fd, i))
                        continue;
                    // we have a response queued, request poll‐out
                    _poll_fds[i].events |= POLLOUT;
                }
            }

            if ((revents & POLLOUT) && !_client_info[current_fd].responses.empty()) {
                sendResponse(current_fd, i);
            }
        }
    }

    Logger::logFrom(LogLevel::INFO, "SocketManager", "Shutting down server");
}

// Accept new client and add to poll list
void SocketManager::handleNewConnection(int listen_fd) {
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) {
        if (errno == EMFILE || errno == ENFILE) {
            // We’ve hit the per‐process or system FD limit.
            Logger::logFrom(
                LogLevel::ERROR, "SocketManager",
                "Out of file descriptors (accept failed: " + std::string(strerror(errno)) + ")");
            return;
        }
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "accept() failed: " + std::string(std::strerror(errno)));
        return;
    }

    if (_poll_fds.size() >= MAX_CLIENTS) {
        Logger::logFrom(LogLevel::WARN, "SocketManager",
                        "Maximum client limit reached. Rejecting connection.");
        close(client_fd); // Optionally send HTTP error before closing (nice but optional) 503
        return;
    }

    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) { // MacOS only
        close(client_fd);
        return; // Shall we log it?
    }

    auto& info               = _client_info[client_fd];
    info.client_fd           = client_fd;
    info.lastRequestTime     = time(NULL);
    info.connectionStartTime = time(NULL);
    info.headerBytesReceived = 0;
    info.bodyBytesReceived   = 0;
    info.headerComplete      = false;
    info.bytes_sent          = 0;
    info.keepAlive           = true;
    info.serverConfig        = _listen_map[listen_fd];

    _poll_fds.push_back((pollfd){client_fd, POLLIN, 0});
    Logger::logFrom(LogLevel::kDEBUG, "SocketManager",
                    "Accept returned fd: " + std::to_string(client_fd) +
                        " | current open clients: " + std::to_string(_client_info.size()));
}

bool hasFullChunkedBody(const std::string& buffer, size_t bodyStart) {
    size_t end = buffer.find("0\r\n\r\n", bodyStart);
    return end != std::string::npos;
}

bool SocketManager::handleCgiRequest(int client_fd, const HttpRequest& request,
                                     const Server& server, const Location& location) {
    ClientInfo& client = _client_info[client_fd];
    client.cgiProcess.emplace();

    int errorCode = 500;
    if (!CGI::initCgiProcess(*client.cgiProcess, request, server, location, _poll_fds, errorCode)) {
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "[CGI] Failed to initialize CGI process for client_fd " +
                            std::to_string(client_fd) + " with script: " + location.getPath());
        HttpResponse err = ResponseBuilder::generateError(errorCode, server, request);
        _client_info[client_fd].responses.push(err);
        client.cgiProcess.reset();
        client.isCgiProcessRunning = false;
        client.currentCgiRequest   = HttpRequest(); // clears request
        return false;                               // error response queued
    }

    return true; // handled as CGI
}

static const Location* findMatchingLocation(const std::string& path, const Server& server) {
    const Location* best = nullptr;
    size_t          max  = 0;
    for (const Location& loc : server.getLocations()) {
        if (path.rfind(normalizePath(loc.getPath()), 0) == 0 &&
            normalizePath(loc.getPath()).size() > max) {
            best = &loc;
            max  = normalizePath(loc.getPath()).size();
        }
    }
    return best;
}

void SocketManager::processPendingRequests(int client_fd) {
    ClientInfo& client = _client_info[client_fd];

    // As long as there is at least one pending request AND no CGI is currently running:
    while (!client.pendingRequests.empty() && !client.isCgiProcessRunning) {
        HttpRequest nextReq = client.pendingRequests.front();

        if (nextReq.getParseErrorCode() != 0) {
            int          code = nextReq.getParseErrorCode();
            HttpResponse err  = ResponseBuilder::generateError(code, client.serverConfig, nextReq);
            client.responses.push(err);
            client.pendingRequests.pop();
            if (err.isConnectionClose())
                return;
            continue;
        }

        // 1) Determine which Location matches
        const Server&   server   = client.serverConfig;
        const Location* location = findMatchingLocation(normalizePath(nextReq.getPath()), server);

        if (!location) {
            // No matching location → 404, enqueue it, then pop pendingRequests
            HttpResponse err = ResponseBuilder::generateError(404, server, nextReq);
            client.responses.push(err);
            client.pendingRequests.pop();

            // If Connection: close, schedule a close:
            if (err.isConnectionClose()) {
                // we’ll close once sendResponse() finishes sending this
                return;
            }
            // Otherwise, keep going to try the next pending request.
            continue;
        }

        // 2) Is it a CGI path?
        std::string resolved = location->resolveAbsolutePath(nextReq.getPath());
        bool        wantCgi  = !resolved.empty() &&
                       (nextReq.getMethod() == "GET" || nextReq.getMethod() == "POST") &&
                       location->isCgiRequest(normalizePath(nextReq.getPath()));

        if (wantCgi) {
            // ── SPAWN A CGI ──
            client.currentCgiRequest   = nextReq;
            client.isCgiProcessRunning = true;

            bool ok = handleCgiRequest(client_fd, nextReq, server, *location);
            // handleCgiRequest(…) should already enqueue an error-response
            // if it fails to fork/exec. In that case, we want to remove this request
            // from pendingRequests anyway, so that we don’t loop infinitely:
            client.pendingRequests.pop();
            if (!ok) {
                continue;
            }
            return; // Stop here. Wait for CGI to finish before doing anything else.
        }

        // 3) Otherwise, it’s a normal static/GET/POST handler:
        HttpResponse resp = handleRequest(nextReq, server);
        client.responses.push(resp);
        client.pendingRequests.pop();

        // If the response says “Connection: close”, we stop here; the connection
        // will be torn down after we send this last response.
        if (resp.isConnectionClose()) {
            return;
        }

        // Otherwise, loop to see if there is another request waiting that can also
        // be immediately turned into a response (so that you can “drain” the queue”).
        continue;
    }
    // If we get here, either pendingRequests is empty, or there’s a CGI in flight.
}

bool SocketManager::handleClientData(int client_fd, size_t index) {
    if (!receiveFromClient(client_fd, index)) {
        return false;
    }
    ClientInfo& client = _client_info[client_fd];
    while (true) {
        if (checkRequestLimits(client_fd)) {
            client.requestBuffer.clear();
            resetRequestState(client_fd);
            return true;
        }
        HttpRequest request;
        int         errorCode     = 0;
        std::size_t consumedBytes = 0;
        bool        parseOK       = HttpRequestParser::parse(request, client.requestBuffer,
                                                             client.serverConfig.getClientMaxBodySize(),
                                                             errorCode, consumedBytes);
        if (!parseOK) {
            if (errorCode == 0) {
                return false; // Incomplete data — wait for more
            } else {
                request.setParseErrorCode(errorCode);
                request.printRequest();
                if (errorCode == 415 || errorCode == 411 || errorCode == 400 || errorCode == 413) {
                    client.requestBuffer.clear();
                } else {
                    client.requestBuffer.erase(0, consumedBytes);
                }
                resetRequestState(client_fd); // DO WE NEED IT?
                client.pendingRequests.push(request);
                // If no more complete request left, break
                if (client.requestBuffer.find("\r\n\r\n") == std::string::npos) {
                    break;
                }
                continue; // We queued a response and continue processing the next request in
                          // pipeline
            }
        }
        client.requestBuffer.erase(0, consumedBytes);
        resetRequestState(client_fd);
        client.pendingRequests.push(request);

        // If no more complete request left, break
        if (_client_info[client_fd].requestBuffer.find("\r\n\r\n") == std::string::npos) {
            break;
        }
    }
    processPendingRequests(client_fd);
    return (true);
}

void SocketManager::sendResponse(int client_fd, size_t index) {
    HttpResponse& response = _client_info[client_fd].responses.front();

    // Log at appropriate level based on status code
    int status = response.getStatusCode();
    if (status >= 200 && status < 400) {
        Logger::logFrom(LogLevel::INFO, "SocketManager sendResponse",
                        "Sending HTTP " + std::to_string(status) + " → fd " +
                            std::to_string(client_fd));
    } else if (status >= 400 && status < 500) {
        Logger::logFrom(LogLevel::WARN, "SocketManager sendResponse",
                        "Sending HTTP " + std::to_string(status) + " → fd " +
                            std::to_string(client_fd));
    } else if (status >= 500) {
        Logger::logFrom(LogLevel::ERROR, "SocketManager sendResponse",
                        "Sending HTTP " + std::to_string(status) + " → fd " +
                            std::to_string(client_fd));
    } else {
        Logger::logFrom(LogLevel::kDEBUG, "SocketManager sendResponse",
                        "Sending HTTP " + std::to_string(status) + " → fd " +
                            std::to_string(client_fd));
    }

    size_t& offset = _client_info[client_fd].bytes_sent;

    if (response.isFileResponse()) {
        // If this is the first time sending this file response, open the file and build headers
        if (!_client_info[client_fd].file_stream.is_open()) {
            _client_info[client_fd].file_stream.open(response.getFilePath(), std::ios::binary);
            if (!_client_info[client_fd].file_stream.is_open()) {
                respondError(client_fd, 500);
                return;
            }

            // If this is a CGI-generated file, skip past the CGI headers in the temp file
            if (response.getCgiBodyOffset() > 0) {
                _client_info[client_fd].file_stream.seekg(response.getCgiBodyOffset());
            }

            // Build the HTTP response head
            std::ostringstream head;
            head << "HTTP/1.1 " << response.getStatusCode() << " " << response.getStatusMessage()
                 << "\r\n";
            for (const auto& header : response.getHeaders()) {
                head << header.first << ": " << header.second << "\r\n";
            }
            head << "\r\n";
            _client_info[client_fd].current_raw_response = head.str();
        }

        // Send any remaining bytes of the HTTP headers first
        std::string& raw = _client_info[client_fd].current_raw_response;
        if (offset < raw.size()) {
            ssize_t sent = send(client_fd, raw.c_str() + offset, raw.size() - offset, MSG_DONTWAIT);
            if (sent < 0) {
                Logger::logFrom(LogLevel::ERROR, "SocketManager",
                                "send() failed on fd " + std::to_string(client_fd) + ": " +
                                    std::strerror(errno));
                cleanupClientConnectionClose(client_fd, index);
                return;
            }
            offset += sent;
            _client_info[client_fd].lastSendAttemptTime = time(NULL);
            if (offset < raw.size()) {
                // Not done sending headers yet; wait for next POLLOUT
                return;
            }
        }

        // Entire header has been sent; now stream the file in 8KB chunks
        char buffer[8192];
        _client_info[client_fd].file_stream.read(buffer, sizeof(buffer));
        std::streamsize bytes_read = _client_info[client_fd].file_stream.gcount();
        if (bytes_read > 0) {
            ssize_t sent = send(client_fd, buffer, bytes_read, MSG_DONTWAIT);
            if (sent < 0) {
                Logger::logFrom(LogLevel::ERROR, "SocketManager",
                                "send() failed on fd " + std::to_string(client_fd) + ": " +
                                    std::strerror(errno));
                cleanupClientConnectionClose(client_fd, index);
                return;
            }
            _client_info[client_fd].lastSendAttemptTime = time(NULL);
            // ─────────────────────────────────────────────────────────────────
            // IMPORTANT: Do NOT modify `offset` here. It tracks only how many
            // bytes of `current_raw_response` have been sent.
            // ─────────────────────────────────────────────────────────────────
        }

        // If EOF reached or zero bytes were read, finish up and clean state
        if (_client_info[client_fd].file_stream.eof() || bytes_read == 0) {
            Logger::logFrom(LogLevel::INFO, "SocketManager sendResponse",
                            "[DONE] We sent full FILE RESPONSE to fd:" + std::to_string(client_fd));
            _client_info[client_fd].file_stream.close();
            _client_info[client_fd].current_raw_response.clear();
            offset = 0;

            // If this was a temporary CGI file, delete it now
            if (response.isCgiTempFile()) {
                CGI::unlinkWithErrorLog(response.getCgiTempFile(), "out temp file");
                response.setCgiTempFile("");
            }

            _client_info[client_fd].responses.pop();

            if (!response.isConnectionClose()) {
                if (_client_info[client_fd].responses.empty()) {
                    Logger::logFrom(LogLevel::INFO, "SocketManager",
                                    "Connection: keep-alive - keeping the connection open");
                    _poll_fds[index].events &= ~POLLOUT;
                }
            } else {
                Logger::logFrom(LogLevel::INFO, "SocketManager",
                                "Connection: close - closing the connection");
                cleanupClientConnectionClose(client_fd, index);
            }
        }

    } else {
        // Non-file responses (e.g., autogenerated bodies) go here
        if (_client_info[client_fd].current_raw_response.empty()) {
            _client_info[client_fd].current_raw_response = response.toHttpString();
            offset                                       = 0;
        }

        std::string& raw = _client_info[client_fd].current_raw_response;
        if (offset < raw.size()) {
            ssize_t bytes_sent =
                send(client_fd, raw.c_str() + offset, raw.size() - offset, MSG_DONTWAIT);
            if (bytes_sent < 0) {
                Logger::logFrom(LogLevel::ERROR, "SocketManager",
                                "send() failed on fd " + std::to_string(client_fd) + ": " +
                                    std::strerror(errno));
                cleanupClientConnectionClose(client_fd, index);
                return;
            }
            offset += bytes_sent;
            _client_info[client_fd].lastSendAttemptTime = time(NULL);
        }

        if (offset >= raw.size()) {
            _client_info[client_fd].responses.pop();
            offset = 0;
            Logger::logFrom(LogLevel::INFO, "SocketManager sendResponse",
                            "[DONE] We sent full RESPONSE to fd:" + std::to_string(client_fd));
            _client_info[client_fd].current_raw_response.clear();
            if (!response.isConnectionClose()) {
                if (_client_info[client_fd].responses.empty()) {
                    Logger::logFrom(LogLevel::INFO, "SocketManager",
                                    "Connection: keep-alive - keeping the connection open");
                    _poll_fds[index].events &= ~POLLOUT;
                }
            } else {
                Logger::logFrom(LogLevel::INFO, "SocketManager",
                                "Connection: close - closing the connection");
                cleanupClientConnectionClose(client_fd, index);
            }
        }
    }
}
