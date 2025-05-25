/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ktieu <ktieu@student.hive.fi>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:51:20 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/25 10:38:24 by ktieu            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "network/SocketManager.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpRequestHandler.hpp"
#include "http/HttpRequestParser.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "utils/buildFilePath.hpp"
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

void SocketManager::resetRequestState(int client_fd) {
    if (!_client_info.count(client_fd))
        return;
    _client_info[client_fd].headerComplete = false;
    _client_info[client_fd].headerBytesReceived = 0;
    _client_info[client_fd].bodyBytesReceived = 0;
}

bool SocketManager::isHeaderTimeout(int fd, time_t now) {
    ClientInfo& client = _client_info[fd];
    if (client.headerBytesReceived < HEADER_MIN_LENGTH &&
        now - client.connectionStartTime > HEADER_TIMEOUT_SECONDS) {
        std::cout << "Header timeout on fd: " << fd << std::endl;
        respondError(fd, 408);
        return true;
    }
    return false;
}

bool SocketManager::isBodyTimeout(int fd, time_t now) {
    ClientInfo& client = _client_info[fd];
    if (client.headerComplete &&
        now - client.lastRequestTime > TIMEOUT) {
        std::cout << "Body timeout on fd: " << fd << std::endl;
        respondError(fd, 408);
        return true;
    }
    return false;
}

bool SocketManager::isSendTimeout(int fd, time_t now) {
    ClientInfo& client = _client_info[fd];
    if (!client.responses.empty() && !client.current_raw_response.empty() &&
        now - client.lastSendAttemptTime > TIMEOUT) {
        std::cout << "Send timeout on fd: " << fd << std::endl;
        return true;
    }
    return false;
}

bool SocketManager::isIdleTimeout(int fd, time_t now) {
    ClientInfo& client = _client_info[fd];
    if (client.responses.empty() && client.current_raw_response.empty() &&
        !client.headerComplete &&
        now - client.lastRequestTime > TIMEOUT) {
        std::cout << "Idle timeout on fd: " << fd << std::endl;
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
        return true;
    }
    return false;
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
/*     std::cout << "======================Received RAW request: {" << buffer << "} bytes: {" << bytes << "}"
              << std::endl;
    std::cout << "==================================================" << std::endl; */

    std::string single_msg(buffer, bytes);
    _client_info[client_fd].requestBuffer += single_msg;
    size_t headerEndPos = _client_info[client_fd].requestBuffer.find("\r\n\r\n");

    if (headerEndPos == std::string::npos) {
        // Header not complete yet
        std::cout << "we didn't find end of header" << std::endl;
        _client_info[client_fd].headerBytesReceived += bytes;
    } else {
        if (!_client_info[client_fd].headerComplete) {
            // First time detecting header end
            std::cout << "we found end of header" << std::endl;
            size_t fullHeaderSize = headerEndPos + 4; // Include "\r\n\r\n"
            size_t oldBufferSize = _client_info[client_fd].requestBuffer.size() - bytes;
            size_t headerBytesThisTime = std::max((ssize_t)0, (ssize_t)(fullHeaderSize - oldBufferSize));
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
    size_t max_size = _client_info[fd].serverConfig.getClientMaxBodySize(); // body check is wrong
    if (_client_info[fd].bodyBytesReceived > max_size ||
        _client_info[fd].headerBytesReceived > HEADER_MAX_LENGTH) {
        std::cout << "Request too large from fd: " << fd << std::endl;
        respondError(fd, 413);
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
        _poll_fds.push_back((pollfd) {fd, POLLIN, 0});
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
            if (checkClientTimeouts(current_fd, i)) {
                _poll_fds[i].events |= POLLOUT; // If connection keep-alive but client idle we close
            }
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

    _poll_fds.push_back((pollfd) {client_fd, POLLIN, 0});
    std::cout << std::endl;
    std::cout << "Accepted client on fd: " << client_fd << std::endl;

    ClientInfo info;
    info.client_fd           = client_fd;
    info.lastRequestTime     = time(NULL);
    info.connectionStartTime = time(NULL);
    info.headerBytesReceived = 0;
    info.bodyBytesReceived   = 0;
    info.headerComplete      = false;
    info.bytes_sent          = 0;
    info.keepAlive           = true;
    info.serverConfig        = _listen_map[listen_fd];

    _client_info[client_fd] = info;
}

bool isValidParsedHeaderMethod(const HttpRequest& request, const Server& server, int& errorCode) {
    static const std::set<std::string> implemented = {"GET", "POST", "DELETE"};

    const std::string& method = request.getMethod();
    const std::string& path   = request.getPath();

     // Find matching location
    const Location* matched     = nullptr;
    size_t          maxMatchLen = 0;

    for (const Location& loc : server.getLocations()) {
        const std::string& locPath = normalizePath(loc.getPath());
        if (path.compare(0, locPath.size(), locPath) == 0 && locPath.size() > maxMatchLen) {
            matched     = &loc;
            maxMatchLen = locPath.size();
        }
    }

    if (!matched) {
        errorCode = 404;
        return false;
    }

    const Location& location = *matched;

    if (!location.isMethodAllowed(method)) {
        errorCode = 405;
        return false;
    }

    return true;
}

// Read data from client, send fixed response, then close
bool SocketManager::handleClientData(int client_fd, size_t index) {
    if (!receiveFromClient(client_fd, index))
        return false;
    if (checkRequestLimits(client_fd)) {
		resetRequestState(client_fd);
		_client_info[client_fd].requestBuffer.clear();
        return true;
	}

    HttpRequest request;
    int         errorCode = 0;
	std::size_t consumedBytes = 0;
    if (!HttpRequestParser::parse(request, _client_info[client_fd].requestBuffer,
                                  _client_info[client_fd].serverConfig.getClientMaxBodySize(),
                                  errorCode, consumedBytes)) {

        if (errorCode == 0){
             std::cout << "Incomplete request, waiting for more data" << std::endl;
/*            std::cout << "_client_info[client_fd].headerComplete: "
                      << _client_info[client_fd].headerComplete << std::endl; */
            /* if (_client_info[client_fd].headerComplete && !isValidParsedHeaderMethod(request, _client_info[client_fd].serverConfig, errorCode)) {
                HttpResponse err = ResponseBuilder::generateError(
                    errorCode, _client_info[client_fd].serverConfig, request);
				resetRequestState(client_fd);
				std::cout << "[1]requestBuffer size is {" << _client_info[client_fd].requestBuffer.size() << "}" << std::endl;
				std::cout << "[1]consumedBytes size is {" << consumedBytes << "}" << std::endl;
				std::cout << "[1]requestBuffer size after erase is {" << _client_info[client_fd].requestBuffer.size() - consumedBytes << "}" << std::endl;
				_client_info[client_fd].requestBuffer.erase(0, consumedBytes);
				_client_info[client_fd].requestBuffer.clear();
                _client_info[client_fd].responses.push(err);
                return true;
            } */
            return false; // Incomplete data — wait for more
        }
        else {
            HttpResponse err = ResponseBuilder::generateError(
                errorCode, _client_info[client_fd].serverConfig, request);
			resetRequestState(client_fd);
			request.printRequest();
			std::cout << "[2]requestBuffer size is {" << _client_info[client_fd].requestBuffer.size() << "}" << std::endl;
				std::cout << "[2]consumedBytes size is {" << consumedBytes << "}" << std::endl;
				std::cout << "[2]requestBuffer size after erase is {" << _client_info[client_fd].requestBuffer.size() - consumedBytes << "}" << std::endl;
			_client_info[client_fd].requestBuffer.erase(0, consumedBytes);
			_client_info[client_fd].requestBuffer.clear();
            _client_info[client_fd].responses.push(err);
            return true; // We queued a response
        }
    }
    request.printRequest();
    resetRequestState(client_fd);
	std::cout << "[3]requestBuffer size is {" << _client_info[client_fd].requestBuffer.size() << "}" << std::endl;
				std::cout << "[3]consumedBytes size is {" << consumedBytes << "}" << std::endl;
				std::cout << "[3]requestBuffer size after erase is {" << _client_info[client_fd].requestBuffer.size() - consumedBytes << "}" << std::endl;
	_client_info[client_fd].requestBuffer.erase(0, consumedBytes);

    const Server& server   = _client_info[client_fd].serverConfig;
    HttpResponse  response = handleRequest(request, server);
    _client_info[client_fd].responses.push(response);

    return (true);
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

    std::cout << "[✅DONE] We sent RESPONSE to fd:" << client_fd << std::endl;
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
