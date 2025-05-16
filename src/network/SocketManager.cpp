/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:51:20 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/17 00:04:52 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "network/SocketManager.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpRequestHandler.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpResponseBuilder.hpp"
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
    setupSockets(servers);
}

SocketManager::~SocketManager() {
    for (const pollfd& pfd : _poll_fds)
        close(pfd.fd);
}

void SocketManager::cleanupClientConnectionClose(int client_fd, size_t index) {
    _poll_fds.erase(_poll_fds.begin() + index);
    _client_info.erase(client_fd);
    close(client_fd);
    std::cout << "We close FD(Connection: close): " << client_fd << std::endl;
}

void SocketManager::checkClientTimeouts(int client_fd, size_t index) {
    if (!_client_info.count(client_fd))
        return;
    time_t now = time(NULL);
    if (_client_info[client_fd].responses.empty() &&
        _client_info[client_fd].current_raw_response.empty() &&
        now - _client_info[client_fd].lastRequestTime > TIMEOUT) {
        std::cout << "Client fd " << client_fd << " timed out is " << TIMEOUT << std::endl;
        cleanupClientConnectionClose(client_fd, index);
        return;
    }
    if (!_client_info[client_fd].responses.empty() &&
        !_client_info[client_fd].current_raw_response.empty() &&
        now - _client_info[client_fd].lastSendAttemptTime > TIMEOUT) {
        std::cout << "Client fd " << client_fd << " too slow to read response (send timeout)"
                  << std::endl;
        cleanupClientConnectionClose(client_fd, index);
    }
}

void SocketManager::handlePollError(int fd, size_t index, short revents) {
    if (revents & POLLERR)
        std::cout << "Socket error on fd: " << fd << std::endl;
    if (revents & POLLHUP)
        std::cout << "Client disconnected (POLLHUP) on fd: " << fd << std::endl;
    cleanupClientConnectionClose(fd, index);
}

bool SocketManager::receiveFromClient(int client_fd, size_t index) {
    char buffer[RECV_BUFFER];
    _client_info[client_fd].lastRequestTime = time(NULL);
    int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0); // MacOS only
    // int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
    if (bytes == 0) {
        std::cout << "Client fd " << client_fd << " disconnected." << std::endl;
        cleanupClientConnectionClose(client_fd, index);
        return false;
    }
    if (bytes < 0) {
        std::cout << "recv() failed: " << std::strerror(errno) << std::endl;
        cleanupClientConnectionClose(client_fd, index);
        return false;
    }
    buffer[bytes] = '\0';
    /* std::cout << "======================Received RAW request: " << buffer << " bytes: " << bytes
              << std::endl;
    std::cout << "==================================================" << std::endl; */
    std::string& bufferRef = _client_info[client_fd].requestBuffer;
    bufferRef.append(buffer, bytes);

    if (_client_info[client_fd].headerBytesReceived == 0) {
        size_t pos = bufferRef.find("\r\n\r\n");
        if (pos != std::string::npos)
            _client_info[client_fd].headerBytesReceived = pos + 4;
    }
    std::string single_msg(buffer, bytes);
    return true;
}

void SocketManager::respondError(int fd, int status_code) {
    HttpRequest  empty;
    HttpResponse err =
        ResponseBuilder::generateError(status_code, _client_info[fd].serverConfig, empty);
    _client_info[fd].responses.push(err);
}

bool SocketManager::checkRequestLimits(int fd) {
    size_t max_body_size = _client_info[fd].serverConfig.getClientMaxBodySize();

    // Enforce header size limit separately
    if (_client_info[fd].headerBytesReceived > HEADER_MAX_LENGTH) {
        std::cout << "Headers too large from fd: " << fd << std::endl;
        respondError(fd, 431); // 431 = Request Header Fields Too Large
        return true;
    }

    // Enforce total request buffer limit (body size)
    if (_client_info[fd].requestBuffer.size() > max_body_size) {
        std::cout << "Body too large from fd: " << fd << std::endl;
        respondError(fd, 413); // 413 = Payload Too Large
        return true;
    }

    return false;
}

bool SocketManager::isHeaderTimeout(int fd) {
    if (_client_info[fd].headerBytesReceived < HEADER_MIN_LENGTH) {
        time_t now = time(NULL);
        if (now - _client_info[fd].connectionStartTime > HEADER_TIMEOUT_SECONDS) {
            std::cout << "Header timeout on fd: " << fd << std::endl;
            respondError(fd, 408);
            return true;
        }
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

        std::cout << "Listening on " << servers[i].getHost() << ":" << servers[i].getPort()
                  << std::endl;
    }
}

// Main server loop using poll
void SocketManager::run() {
    while (running) {
        int n = poll(&_poll_fds[0], _poll_fds.size(), 1000); // Poll each 1sec (timeout = 1sec)
        if (n < 0) {
            if (errno ==
                EINTR) { // we can try to handle signal here (A signal was caught during poll().)
                running = 0;
                continue;
            }
            throw SocketError("poll() failed: " + std::string(std::strerror(errno)));
        }

        for (size_t i = _poll_fds.size(); i-- > 0;) {
            short revents    = _poll_fds[i].revents;
            int   current_fd = _poll_fds[i].fd;
            if (revents & POLLERR || revents & POLLHUP) {
                handlePollError(current_fd, i, revents);
                continue;
            }
            if (revents & POLLIN) { // Ready to read (incoming data or connection)
                if (_listen_map.count(current_fd))
                    handleNewConnection(current_fd);
                else {
                    if (!handleClientData(current_fd, i))
                        continue;
                    // After handling the request, mark the socket as ready for writing (POLLOUT)
                    _poll_fds[i].events |= POLLOUT;
                }
            }
            if ((revents & POLLOUT) &&
                !_client_info[current_fd].responses.empty()) { // Ready to write (can send data)
                sendResponse(current_fd, i);
            }
            checkClientTimeouts(current_fd, i); // If connection keep-alive but client idle we close
        }
    }
    std::cout << std::endl;
    std::cout << "Shutting down server" << std::endl;
}

// Accept new client and add to poll list
void SocketManager::handleNewConnection(int listen_fd) {
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0)
        return; // Shall we log it?

    if (_poll_fds.size() >= MAX_CLIENTS) {
        std::cout << "Maximum client limit reached. Rejecting connection." << std::endl;
        close(client_fd); // Optionally send HTTP error before closing (nice but optional) 503
        return;
    }

    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) { // MacOS only
        close(client_fd);
        return; // Shall we log it?
    }

    _poll_fds.push_back((pollfd){client_fd, POLLIN, 0});
    std::cout << std::endl;
    std::cout << "Accepted client on fd: " << client_fd << std::endl;

    ClientInfo info;
    info.client_fd           = client_fd;
    info.lastRequestTime     = time(NULL);
    info.connectionStartTime = time(NULL);
    info.headerBytesReceived = 0;
    info.bytes_sent          = 0;
    info.keepAlive           = true;
    info.serverConfig        = _listen_map[listen_fd];

    _client_info[client_fd] = info;
}

bool hasFullChunkedBody(const std::string& buffer, size_t bodyStart) {
    size_t end = buffer.find("0\r\n\r\n", bodyStart);
    return end != std::string::npos;
}

// Read data from client, send fixed response, then close
bool SocketManager::handleClientData(int client_fd, size_t index) {
    if (!receiveFromClient(client_fd, index))
        return false;

    if (checkRequestLimits(client_fd))
        return true;

    if (isHeaderTimeout(client_fd))
        return true;

    std::string& buffer     = _client_info[client_fd].requestBuffer;
    size_t       headersEnd = buffer.find("\r\n\r\n");

    // Wait for complete headers
    if (headersEnd == std::string::npos)
        return false;

    std::string headersPart = buffer.substr(0, headersEnd + 4);

    HttpRequest tmpRequest;
    if (!tmpRequest.parseHeadersOnly(headersPart)) {
        respondError(client_fd, 400);
        return true;
    }

    std::string contentLengthStr = tmpRequest.getHeader("Content-Length");
    std::string transferEncoding = tmpRequest.getHeader("Transfer-Encoding");

    size_t bodyStart = headersEnd + 4;
    size_t totalSize = buffer.size();

    // Handle Content-Length (fixed-length body)
    if (!contentLengthStr.empty()) {
        size_t contentLength = std::strtoul(contentLengthStr.c_str(), NULL, 10);
        if (totalSize < bodyStart + contentLength)
            return false; // wait for full body
    }
    // Handle chunked body
    else if (transferEncoding == "chunked") {
        if (!hasFullChunkedBody(buffer, bodyStart))
            return false; // wait for 0\r\n\r\n
    }
    // Else: no body → proceed

    // Parse full request now
    HttpRequest request;
    if (!request.parse(buffer)) {
        std::cerr << "Failed to parse HTTP request.\n";
        HttpResponse badRequest =
            ResponseBuilder::generateError(400, _client_info[client_fd].serverConfig, request);
        _client_info[client_fd].responses.push(badRequest);
        return true;
    }

    request.printRequest();
    buffer.clear();

    const Server& server   = _client_info[client_fd].serverConfig;
    HttpResponse  response = handleRequest(request, server);
    _client_info[client_fd].responses.push(response);

    return true;
}

// Accept new client and add to poll list
void SocketManager::sendResponse(int client_fd, size_t index) {
    HttpResponse response = _client_info[client_fd].responses.front();

    if (_client_info[client_fd].current_raw_response.empty()) {
        _client_info[client_fd].current_raw_response = response.toHttpString();
        _client_info[client_fd].bytes_sent           = 0;
    }

    std::string& raw          = _client_info[client_fd].current_raw_response;
    size_t       sent_already = _client_info[client_fd].bytes_sent;

    // ssize_t bytes_sent = send(client_fd, raw.c_str(), raw.size(), 0); // MacOS only
    ssize_t bytes_sent =
        send(client_fd, raw.c_str() + sent_already, raw.size() - sent_already, MSG_DONTWAIT);
    if (bytes_sent < 0) {
        std::cerr << "send() failed on fd " << client_fd << ": " << std::strerror(errno)
                  << std::endl;
        cleanupClientConnectionClose(client_fd, index);
        return;
    }

    std::cout << "=======================We sent to fd:" << client_fd << std::endl;
    std::cout << raw << std::endl;
    std::cout << "==================================================" << std::endl;

    _client_info[client_fd].bytes_sent += bytes_sent;
    _client_info[client_fd].lastSendAttemptTime = time(NULL);
    if (_client_info[client_fd].bytes_sent == raw.size()) {
        _client_info[client_fd].responses.pop();
        _client_info[client_fd].current_raw_response.clear();
        _client_info[client_fd].bytes_sent = 0;

        if (!response.isConnectionClose()) {
            std::cout << "Connection: keep-alive - keeping the connection open" << std::endl;
            // We should not close the client connection, but just reset the POLLOUT flag if needed
            _poll_fds[index].events &=
                ~POLLOUT; // Reset POLLOUT flag if the connection should stay open
        } else {
            // If it's not keep-alive, close the connection
            std::cout << "Connection: close - closing the connection" << std::endl;
            cleanupClientConnectionClose(client_fd, index);
        }
    }
    // else: partial send, keep waiting for POLLOUT and continue sending later
}
