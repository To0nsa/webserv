/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:51:20 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/08 19:06:07 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "network/SocketManager.hpp"
#include "utils/Logger.hpp"          // for LogLevel, Logger
#include "utils/filesystemUtils.hpp" // for getCurrentTime
#include <arpa/inet.h>               // for inet_addr, htons
#include <bits/types/sig_atomic_t.h> // for sig_atomic_t
#include <cstring>                   // for strerror, NULL
#include <errno.h>                   // for errno, EINTR, EMFILE, ENFILE
#include <netinet/in.h>              // for sockaddr_in, in_addr
#include <new>                       // for bad_alloc
#include <set>                       // for set
#include <signal.h>                  // for size_t, signal, SIGINT, SIGPIPE
#include <sys/socket.h>              // for AF_INET, accept, bind, listen
#include <unistd.h>                  // for close
#include <utility>                   // for pair, make_pair

static volatile sig_atomic_t running = 1;

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

// Custom exception for socket errors
SocketManager::SocketError::SocketError(const std::string& msg) {
    _msg = msg;
}

const char* SocketManager::SocketError::what() const throw() {
    return (_msg.c_str());
}

// Set up sockets for each server (host:port)
void SocketManager::setupSockets(const std::vector<Server>& servers) {
    std::set<std::pair<std::string, int>> bound;
    for (size_t i = 0; i < servers.size(); ++i) {
        const std::string& host = servers[i].getHost();
        int                port = servers[i].getPort();

        std::pair<std::string, int> key = std::make_pair(host, port);
        if (bound.count(key))
            continue; // Already bound, skip
        bound.insert(key);

        int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (fd < 0)
            throw SocketError("socket() failed: " + std::string(std::strerror(errno)));

        int opt = 1; // Reuse same address/port if the server is restarted quickly
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            close(fd);
            throw SocketError("setsockopt() failed: " + std::string(std::strerror(errno)));
        }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port); // Convert port to network byte order
        // Convert hostname to IP address
        if (host == "localhost")
            addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        else
            addr.sin_addr.s_addr = inet_addr(host.c_str());

        if (bind(fd, (sockaddr*) &addr, sizeof(addr)) < 0) { // Bind socket to IP:port
            close(fd);
            throw SocketError("bind() failed on " + host + ":" + std::to_string(port) + ": " +
                              strerror(errno));
        }

        if (listen(fd, SOMAXCONN) < 0) { // Start listening for incoming connections
            close(fd);
            throw SocketError("listen() failed: " + std::string(std::strerror(errno)));
        }

        // Register fd in poll list
        _poll_fds.push_back((pollfd){fd, POLLIN, 0});
        // Collect all servers for this host:port
        std::vector<Server> vhosts;
        for (size_t j = 0; j < servers.size(); ++j) {
            if (servers[j].getHost() == host && servers[j].getPort() == port)
                vhosts.push_back(servers[j]);
        }

        _listen_map[fd] = vhosts;
        Logger::logFrom(LogLevel::INFO, "SocketManager",
                        "Listening on " + host + ":" + std::to_string(port));
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

        handleCgiPollEvents();

        // Then handle sockets — but skip *all* CGI FDs before doing error/HUP checks
        for (size_t i = _poll_fds.size(); i-- > 0;) {
            try {
                short revents    = _poll_fds[i].revents;
                int   current_fd = _poll_fds[i].fd;
                if (checkClientTimeouts(current_fd, i)) {
                    _poll_fds[i].events |=
                        POLLOUT; // If connection keep-alive but client idle we close
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
            } catch (const std::exception& e) {
                Logger::logFrom(LogLevel::ERROR, "SocketManager",
                                "Exception in poll loop for fd: " +
                                    std::to_string(_poll_fds[i].fd) + ": " + e.what());
                cleanupClientConnectionClose(_poll_fds[i].fd, i);
            } catch (...) {
                Logger::logFrom(LogLevel::ERROR, "SocketManager",
                                "Unknown exception in poll loop for fd: " +
                                    std::to_string(_poll_fds[i].fd));
                cleanupClientConnectionClose(_poll_fds[i].fd, i);
            }
        }
    }

    Logger::logFrom(LogLevel::INFO, "SocketManager", "Shutting down server");
}

void SocketManager::initializeClientInfo(int client_fd, int listen_fd) {
    ClientInfo& info = _client_info[client_fd];

    info.client_fd           = client_fd;
    info.lastRequestTime     = getCurrentTime();
    info.connectionStartTime = getCurrentTime();
    info.headerBytesReceived = 0;
    info.bodyBytesReceived   = 0;
    info.headerComplete      = false;
    info.bytes_sent          = 0;
    info.serversOnPort       = _listen_map[listen_fd];
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

    initializeClientInfo(client_fd, listen_fd);
    _poll_fds.push_back((pollfd){client_fd, POLLIN, 0});
    Logger::logFrom(LogLevel::kDEBUG, "SocketManager",
                    "Accept returned fd: " + std::to_string(client_fd) +
                        " | current open clients: " + std::to_string(_client_info.size()));
}

bool SocketManager::handleClientData(int client_fd, size_t index) {
    try {
        if (!receiveFromClient(client_fd, index))
            return false;
        if (!parseAndQueueRequests(client_fd))
            return false;
        processPendingRequests(client_fd);
        return (true);
    } catch (const std::bad_alloc& e) {
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "Memory allocation failed while handling client " +
                            std::to_string(client_fd) + ": " + e.what());
    } catch (const std::exception& e) {
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "Exception while handling client " + std::to_string(client_fd) + ": " +
                            e.what());
    } catch (...) {
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "Unknown exception while handling client " + std::to_string(client_fd));
    }
    _poll_fds[index].events &= ~POLLIN;
    respondError(client_fd, 500);
    return (true);
}
