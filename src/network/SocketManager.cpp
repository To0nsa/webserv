/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:51:20 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/25 21:56:10 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "network/SocketManager.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpRequestHandler.hpp"
#include "http/HttpRequestParser.hpp"
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

void SocketManager::cleanupCgiForClient(int client_fd) {
    if (!_client_info.contains(client_fd))
        return;

    ClientInfo& client = _client_info[client_fd];
    if (!client.cgiProcess)
        return;

    const CgiProcess& cgi = *client.cgiProcess;

    // Remove from fd→cgi map
    _fd_to_cgi.erase(cgi.stdin_fd);
    _fd_to_cgi.erase(cgi.stdout_fd);

    // Remove fds from poll
    _poll_fds.erase(std::remove_if(_poll_fds.begin(), _poll_fds.end(),
                                   [&](const pollfd& pfd) {
                                       return pfd.fd == cgi.stdin_fd || pfd.fd == cgi.stdout_fd;
                                   }),
                    _poll_fds.end());

    CGI::cleanupCgi(*client.cgiProcess);
    client.cgiProcess.reset();
}

void SocketManager::cleanupClientConnectionClose(int client_fd, size_t index) {
    cleanupCgiForClient(client_fd); // new line
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
    if (client.responses.empty() && client.current_raw_response.empty() &&
        client.headerBytesReceived < HEADER_MIN_LENGTH &&
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
        now - client.connectionStartTime > TIMEOUT) {
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
        _poll_fds[index].events &= ~POLLIN;
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
		if (_client_info[client_fd].headerBytesReceived == 0) {
			_client_info[client_fd].connectionStartTime = time(NULL);
		}
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
    if (_client_info[fd].headerBytesReceived > HEADER_MAX_LENGTH) {
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
        _poll_fds.push_back((pollfd){fd, POLLIN, 0});
        _listen_map[fd] = servers[i]; // Map fd to its corresponding server

        std::cout << "Listening on " << servers[i].getHost() << ":" << servers[i].getPort()
                  << std::endl;
    }
}

void SocketManager::handleCgiPollEvents() {
    // Helper: once we queue a response, ensure poll() will wake on POLLOUT
    auto markClientWritable = [&](int client_fd) {
        for (auto& pfd : _poll_fds) {
            if (pfd.fd == client_fd) {
                pfd.events |= POLLOUT;
                break;
            }
        }
    };

    // Walk backwards so erasing entries is safe
    for (size_t i = _poll_fds.size(); i-- > 0;) {
        auto& pfd     = _poll_fds[i];
        int   fd      = pfd.fd;
        short revents = pfd.revents;

        // Only handle CGI pipe FDs here
        if (!_fd_to_cgi.contains(fd))
            continue;

        auto [client_fd, which] = _fd_to_cgi.at(fd);
        ClientInfo& client      = _client_info[client_fd];
        if (!client.cgiProcess)
            continue;

        CgiProcess& cgi = *client.cgiProcess;
        time_t      now = time(NULL);

        // 1) Timeout guard (5s)
        if (now - cgi.last_activity > 0.5) {
            std::cerr << "[CGI] Timeout on fd " << fd << " for client_fd " << client_fd << "\n";
            client.responses.push(ResponseBuilder::generateError(504, client.serverConfig, {}));
            markClientWritable(client_fd);

            CGI::cleanupCgi(cgi);
            client.cgiProcess.reset();
            _poll_fds.erase(_poll_fds.begin() + i);
            _fd_to_cgi.erase(fd);
            continue;
        }

        bool success = true;
        // 2) Drive CGI stdin
        if (cgi.phase == CgiProcess::Phase::Writing && which == "stdin" && (revents & POLLOUT)) {
            success = CGI::handleWrite(cgi);
        }
        // 3) Drive CGI stdout (data or EOF)
        if (cgi.phase == CgiProcess::Phase::Reading && which == "stdout") {
            if (revents & POLLIN) {
                success = CGI::handleRead(cgi);
            }
            if (revents & POLLHUP) {
                std::cerr << "[CGI DEBUG] POLLHUP on stdout FD=" << fd << " → setting phase=Done\n";
                cgi.phase = CgiProcess::Phase::Done;
            }
        }

        // 4) On I/O error
        if (!success) {
            std::cerr << "[CGI] I/O error on fd " << fd << "\n";
            client.responses.push(ResponseBuilder::generateError(502, client.serverConfig, {}));
            markClientWritable(client_fd);

            CGI::cleanupCgi(cgi);
            client.cgiProcess.reset();
            _poll_fds.erase(_poll_fds.begin() + i);
            _fd_to_cgi.erase(fd);
            continue;
        }

        // 5) Finalize when done
        if (cgi.phase == CgiProcess::Phase::Done) {
            std::cerr << "[CGI] Phase Done, checking child status...\n";
            if (!CGI::tryTerminateCgi(cgi)) {
                // child not reaped yet → come back next loop
                continue;
            }

            // build and queue the HTTP response
            auto maybeResp = CGI::finalizeCgi(cgi, client.serverConfig, {/*req*/});
            auto resp =
                maybeResp.value_or(ResponseBuilder::generateError(502, client.serverConfig, {}));
            client.responses.push(resp);
            markClientWritable(client_fd);

            // clean up both pipe FDs at once
            std::erase_if(_poll_fds, [&](auto const& p) {
                return p.fd == cgi.stdin_fd || p.fd == cgi.stdout_fd;
            });
            _fd_to_cgi.erase(cgi.stdin_fd);
            _fd_to_cgi.erase(cgi.stdout_fd);
            client.cgiProcess.reset();
        }
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

        // First, drive any pending CGI reads/writes
        handleCgiPollEvents();

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
            if (revents & POLLERR || revents & POLLHUP) {
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
                    pfd.events |= POLLOUT;
                }
            }

            if ((revents & POLLOUT) && !_client_info[current_fd].responses.empty()) {
                sendResponse(current_fd, i);
            }
        }
    }

    std::cout << "\nShutting down server\n";
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
    info.bodyBytesReceived   = 0;
    info.headerComplete      = false;
    info.bytes_sent          = 0;
    info.keepAlive           = true;
    info.serverConfig        = _listen_map[listen_fd];

    _client_info[client_fd] = info;
}

bool hasFullChunkedBody(const std::string& buffer, size_t bodyStart) {
    size_t end = buffer.find("0\r\n\r\n", bodyStart);
    return end != std::string::npos;
}

bool SocketManager::handleCgiRequest(int client_fd, const HttpRequest& request,
                                     const Server& server, const Location& location) {
    ClientInfo& client = _client_info[client_fd];
    client.cgiProcess.emplace();

    if (!CGI::initCgiProcess(*client.cgiProcess, request, server, location)) {
        respondError(client_fd, 500);
        client.cgiProcess.reset();
        return true; // error response queued
    }

    const CgiProcess& cgi = *client.cgiProcess;

    _poll_fds.push_back({cgi.stdin_fd, POLLOUT, 0});
    _poll_fds.push_back({cgi.stdout_fd, POLLIN, 0});

    _fd_to_cgi[cgi.stdin_fd]  = {client_fd, "stdin"};
    _fd_to_cgi[cgi.stdout_fd] = {client_fd, "stdout"};

    return true; // handled as CGI
}

static const Location* findMatchingLocation(const std::string& path, const Server& server) {
    const Location* best = nullptr;
    size_t          max  = 0;
    for (const Location& loc : server.getLocations()) {
        if (path.rfind(loc.getPath(), 0) == 0 && loc.getPath().size() > max) {
            best = &loc;
            max  = loc.getPath().size();
        }
    }
    return best;
}

bool SocketManager::handleClientData(int client_fd, size_t index) {
    if (!receiveFromClient(client_fd, index)) {
        return false;
	}
	while (true) {
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
				_client_info[client_fd].responses.push(err);
				// If keep-alive is false, break the loop to close connection
				if (err.isConnectionClose()) {
					break;
				}
		
				// If no more complete request left, break
				if (_client_info[client_fd].requestBuffer.find("\r\n\r\n") == std::string::npos) {
					break;
				}
				continue; // We queued a response and continue processing the next request in pipeline
			}
		}
		request.printRequest();
		resetRequestState(client_fd);
		std::cout << "[3]requestBuffer size is {" << _client_info[client_fd].requestBuffer.size() << "}" << std::endl;
		std::cout << "[3]consumedBytes size is {" << consumedBytes << "}" << std::endl;
		std::cout << "[3]requestBuffer size after erase is {" << _client_info[client_fd].requestBuffer.size() - consumedBytes << "}" << std::endl;
		_client_info[client_fd].requestBuffer.erase(0, consumedBytes);

		const Server&   server   = _client_info[client_fd].serverConfig;
        const Location* location = findMatchingLocation(request.getPath(), server);

        if (!location) {
            respondError(client_fd, 404);
            return true;
        }

        if (!location->isMethodAllowed(request.getMethod())) {
            respondError(client_fd, 405);
            return true;
        }

        if (location->isCgiRequest(request.getPath())) {
            return handleCgiRequest(client_fd, request, server, *location);
        }

        // Fallback to standard GET/POST/DELETE handler
		HttpResponse  response = handleRequest(request, server);
		_client_info[client_fd].responses.push(response);
		// If keep-alive is false, break the loop to close connection
        if (response.isConnectionClose()) {
            break;
        }

        // If no more complete request left, break
        if (_client_info[client_fd].requestBuffer.find("\r\n\r\n") == std::string::npos) {
            break;
        }
	}

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
			if (_client_info[client_fd].responses.empty()) {
            // We should not close the client connection, but just reset the POLLOUT flag if needed
            _poll_fds[index].events &=
                ~POLLOUT; // Reset POLLOUT flag if the connection should stay open
			}
        } else {
            // If it's not keep-alive, close the connection
            std::cout << "Connection: close - closing the connection" << std::endl;
            cleanupClientConnectionClose(client_fd, index);
        }
    }
    // else: partial send, keep waiting for POLLOUT and continue sending later
}
