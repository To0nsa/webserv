/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:51:20 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/18 22:51:52 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    SocketManager.cpp
 * @brief   Implements the SocketManager core loop and server socket setup.
 *
 * @details
 * Provides the main event-driven networking logic of Webserv.
 * - Sets up listening sockets for configured servers.
 * - Runs the non-blocking poll() loop to multiplex all I/O.
 * - Handles new connections, client requests, and responses.
 * - Integrates CGI handling, timeout checks, and error recovery.
 *
 * This file contains the **control flow backbone** of the web server:
 * it ties together sockets, HTTP parsing, response building, CGI execution,
 * and cleanup into a single poll-driven loop.
 *
 * @ingroup socker_mananager
 */

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

/// @internal
/// @brief Global flag controlling the server loop.
/// @details Set to `0` when SIGINT is received, causing the main loop to exit.
static volatile sig_atomic_t running = 1;

/**
 * @internal
 * @brief Signal handler for process-level interrupts.
 *
 * @param signum Signal number received.
 *
 * @details
 * - On `SIGINT` (Ctrl+C), sets the global `running` flag to `0`
 *   to trigger a graceful shutdown.
 * - `SIGPIPE` is ignored in the constructor (see @ref SocketManager),
 *   to prevent crashes when clients disconnect abruptly.
 */
static void signalHandler(int signum) {
    if (signum == SIGINT)
        running = 0;
}

/**
 * @brief Constructs a new SocketManager.
 *
 * @details
 * - Installs signal handlers (`SIGINT` for shutdown, `SIGPIPE` ignored).
 * - Initializes listening sockets for all configured servers.
 *
 * @param servers List of configured @ref Server instances.
 *
 * @throws SocketError if socket creation, binding, or listening fails.
 */
SocketManager::SocketManager(const std::vector<Server>& servers) {
    signal(SIGINT, signalHandler);
    signal(SIGPIPE, SIG_IGN);
    setupSockets(servers);
}

/**
 * @brief Destructor for SocketManager.
 *
 * @details
 * Closes all file descriptors tracked in the poll set to
 * ensure a clean shutdown.
 */
SocketManager::~SocketManager() {
    for (const pollfd& pfd : _poll_fds)
        close(pfd.fd);
}

/**
 * @brief Handles error or hangup events reported by poll().
 *
 * @details
 * Interprets the poll revents mask for a given client file descriptor
 * and logs the corresponding error type:
 * - `POLLNVAL`: Invalid poll event on this fd.
 * - `POLLERR`: Generic socket error.
 * - `POLLHUP`: Client disconnected.
 *
 * After logging, the client connection is closed and cleaned up
 * via @ref cleanupClientConnectionClose.
 *
 * @param fd     The file descriptor that triggered the error event.
 * @param index  Index of the fd in the internal poll list.
 * @param revents The bitmask of poll events received for the fd.
 */
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

/**
 * @brief Initializes and binds listening sockets for all configured servers.
 *
 * @details
 * Iterates over the provided @ref Server instances and:
 * - Ensures each (host, port) pair is only bound once.
 * - Creates a non-blocking TCP socket (`SOCK_STREAM | SOCK_NONBLOCK`).
 * - Enables `SO_REUSEADDR` to allow quick restarts on the same port.
 * - Binds the socket to the host:port.
 * - Puts the socket into listening mode with `listen()`.
 * - Registers the socket in the internal poll list for event monitoring.
 * - Groups servers sharing the same host:port into a virtual host list.
 *
 * @param servers List of servers to initialize listening sockets for.
 *
 * @throws SocketError If socket creation, setsockopt, bind, or listen fails.
 *
 * @note
 * - "localhost" is explicitly mapped to `127.0.0.1`.
 * - Duplicate host:port entries are skipped to avoid double-binding.
 */
void SocketManager::setupSockets(const std::vector<Server>& servers) {
    std::set<std::pair<std::string, int>> bound; // Track already-bound host:port pairs

    for (size_t i = 0; i < servers.size(); ++i) {
        const std::string& host = servers[i].getHost();
        int                port = servers[i].getPort();

        // Skip if this host:port has already been bound
        std::pair<std::string, int> key = std::make_pair(host, port);
        if (bound.count(key))
            continue;
        bound.insert(key);

        // Create a non-blocking TCP socket
        int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (fd < 0)
            throw SocketError("socket() failed: " + std::string(std::strerror(errno)));

        // Allow socket address reuse to restart quickly after shutdown
        int opt = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            close(fd);
            throw SocketError("setsockopt() failed: " + std::string(std::strerror(errno)));
        }

        // Prepare socket address structure
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port); // Convert port to network byte order

        // Map "localhost" to loopback explicitly, otherwise use given host string
        if (host == "localhost")
            addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        else
            addr.sin_addr.s_addr = inet_addr(host.c_str());

        // Bind socket to the host:port
        if (bind(fd, (sockaddr*) &addr, sizeof(addr)) < 0) {
            close(fd);
            throw SocketError("bind() failed on " + host + ":" + std::to_string(port) + ": " +
                              strerror(errno));
        }

        // Put socket into listening mode
        if (listen(fd, SOMAXCONN) < 0) {
            close(fd);
            throw SocketError("listen() failed: " + std::string(std::strerror(errno)));
        }

        // Register fd in poll list for event monitoring
        _poll_fds.push_back((pollfd){fd, POLLIN, 0});

        // Collect all servers configured on this host:port (virtual hosts)
        std::vector<Server> vhosts;
        for (size_t j = 0; j < servers.size(); ++j) {
            if (servers[j].getHost() == host && servers[j].getPort() == port)
                vhosts.push_back(servers[j]);
        }

        // Associate this listening fd with its vhost set
        _listen_map[fd] = vhosts;

        Logger::logFrom(LogLevel::INFO, "SocketManager",
                        "Listening on " + host + ":" + std::to_string(port));
    }
}

/**
 * @brief Runs the main event loop of the server.
 *
 * @details
 * This method drives the **non-blocking, poll()-based event loop** of Webserv.
 * - Uses a single `poll()` call to multiplex all I/O (listening sockets, client FDs, CGI pipes).
 * - Periodically checks for CGI activity and client timeouts.
 * - Handles:
 *   - New connections (on listening sockets),
 *   - Read events (incoming client data),
 *   - Write events (pending responses),
 *   - Error/hangup events (POLLERR, POLLHUP, POLLNVAL).
 * - Ensures resilience with try/catch around per-FD handling.
 *
 * The loop continues until the global `running` flag is cleared (SIGINT).
 *
 * @throws SocketError If `poll()` fails unexpectedly (other than EINTR).
 */
void SocketManager::run() {
    while (running) {
        // Wait up to 1s for activity on any registered FD
        int n = poll(&_poll_fds[0], _poll_fds.size(), 1000);
        if (n < 0) {
            if (errno == EINTR) {
                // Interrupted by signal → trigger graceful shutdown
                running = 0;
                continue;
            }
            throw SocketError("poll() failed: " + std::string(std::strerror(errno)));
        }

        // First, handle any active CGI processes (timeout, completion, cleanup)
        handleCgiPollEvents();

        // Process poll events for each file descriptor in reverse order
        for (size_t i = _poll_fds.size(); i-- > 0;) {
            try {
                short revents    = _poll_fds[i].revents; // events that fired
                int   current_fd = _poll_fds[i].fd;

                // Check if this client has hit a timeout (idle/header/body/send)
                if (checkClientTimeouts(current_fd, i)) {
                    // Mark for POLLOUT → we will close after sending error/timeout response
                    _poll_fds[i].events |= POLLOUT;
                }

                // Skip CGI pipe descriptors entirely — they’re handled in handleCgiPollEvents()
                if (_fd_to_cgi.contains(current_fd)) {
                    continue;
                }

                // Handle socket-level errors or hangups
                if (revents & POLLERR || revents & POLLHUP || revents & POLLNVAL) {
                    handlePollError(current_fd, i, revents);
                    continue;
                }

                // Incoming data available
                if (revents & POLLIN) {
                    if (_listen_map.count(current_fd)) {
                        // This is a listening socket → accept new connection
                        handleNewConnection(current_fd);
                    } else {
                        // Existing client → read request
                        if (!handleClientData(current_fd, i))
                            continue; // client closed or error, skip further handling
                        // At least one response is now queued → request POLLOUT
                        _poll_fds[i].events |= POLLOUT;
                    }
                }

                // Ready to write response
                if ((revents & POLLOUT) && !_client_info[current_fd].responses.empty()) {
                    sendResponse(current_fd, i);
                }
            } catch (const std::exception& e) {
                // Per-FD error resilience: log and cleanup
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

/**
 * @brief Initializes tracking information for a newly accepted client.
 *
 * @details
 * Creates or resets a @ref ClientInfo entry for the given client file descriptor:
 * - Stores the client FD for later reference.
 * - Initializes timestamps (`lastRequestTime`, `connectionStartTime`).
 * - Resets request/response state (header/body counters, completion flags).
 * - Clears send progress tracking (`bytes_sent`).
 * - Associates the client with all servers (virtual hosts) listening on the
 *   same port as the accept()'ed socket.
 *
 * @param client_fd File descriptor of the newly accepted client socket.
 * @param listen_fd File descriptor of the listening socket that accepted this client.
 */
void SocketManager::initializeClientInfo(int client_fd, int listen_fd) {
    ClientInfo& info = _client_info[client_fd]; // Create/lookup client state entry

    // Basic identification
    info.client_fd = client_fd;

    // Initialize timers for connection and activity
    info.lastRequestTime     = getCurrentTime();
    info.connectionStartTime = getCurrentTime();

    // Reset counters and parsing state
    info.headerBytesReceived = 0;
    info.bodyBytesReceived   = 0;
    info.headerComplete      = false;

    // Reset response send tracking
    info.bytes_sent = 0;

    // Assign list of servers bound to the same listen_fd (vhost support)
    info.serversOnPort = _listen_map[listen_fd];
}

/**
 * @brief Accepts and registers a new client connection.
 *
 * @details
 * Called when a listening socket signals readiness (`POLLIN`):
 * - Calls `accept()` to retrieve a new client socket FD.
 * - Handles resource exhaustion gracefully:
 *   - `EMFILE` / `ENFILE`: process/system FD limit reached → log and drop.
 *   - `MAX_CLIENTS` limit: too many clients → reject and close FD.
 * - Initializes the new client state via @ref initializeClientInfo.
 * - Adds the client FD to the poll set, monitoring for read events.
 *
 * @param listen_fd The listening socket FD on which the connection was accepted.
 */
void SocketManager::handleNewConnection(int listen_fd) {
    // Accept a pending connection on the listening socket
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) {
        if (errno == EMFILE || errno == ENFILE) {
            // System or process has run out of file descriptors
            Logger::logFrom(
                LogLevel::ERROR, "SocketManager",
                "Out of file descriptors (accept failed: " + std::string(strerror(errno)) + ")");
            return;
        }
        // Generic accept() failure
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "accept() failed: " + std::string(std::strerror(errno)));
        return;
    }

    // Enforce application-level connection limit
    if (_poll_fds.size() >= MAX_CLIENTS) {
        Logger::logFrom(LogLevel::WARN, "SocketManager",
                        "Maximum client limit reached. Rejecting connection.");
        close(client_fd); // Could optionally send an HTTP 503 before closing
        return;
    }

    // Initialize per-client state tracking
    initializeClientInfo(client_fd, listen_fd);

    // Register client FD into poll list, listening for incoming data
    _poll_fds.push_back((pollfd){client_fd, POLLIN, 0});

    // Debug log: report accepted client and total open connections
    Logger::logFrom(LogLevel::kDEBUG, "SocketManager",
                    "Accept returned fd: " + std::to_string(client_fd) +
                        " | current open clients: " + std::to_string(_client_info.size()));
}

/**
 * @brief Processes incoming data for a client connection.
 *
 * @details
 * Called when a client socket is readable (`POLLIN`):
 * 1. Reads available data from the client using @ref receiveFromClient.
 *    - If the client disconnects or a read error occurs → return `false` (FD will be closed).
 * 2. Attempts to parse one or more complete HTTP requests from the buffer
 *    via @ref parseAndQueueRequests.
 *    - If parsing is incomplete, waits for more data.
 *    - If a parse error occurs, queues an error response.
 * 3. Processes all queued requests (except those blocked by CGI execution)
 *    via @ref processPendingRequests.
 *
 * Exception safety:
 * - Catches `std::bad_alloc` (memory exhaustion), `std::exception`, and unknown errors.
 * - On error: disables further POLLIN events for this client and queues a `500 Internal Server
 * Error`.
 *
 * @param client_fd File descriptor of the client socket.
 * @param index     Index of the client FD in the internal poll list.
 *
 * @return `true` if processing completed (even with errors queued),
 *         `false` if the connection should be closed.
 */
bool SocketManager::handleClientData(int client_fd, size_t index) {
    try {
        // 1) Receive raw bytes from the client socket
        if (!receiveFromClient(client_fd, index))
            return false; // Client disconnected or fatal recv() error

        // 2) Parse requests and queue them for processing
        if (!parseAndQueueRequests(client_fd))
            return false; // Incomplete request → wait for more data

        // 3) Execute queued requests (routing, CGI, response building)
        processPendingRequests(client_fd);

        return true;
    } catch (const std::bad_alloc& e) {
        // Out-of-memory while processing this client
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "Memory allocation failed while handling client " +
                            std::to_string(client_fd) + ": " + e.what());
    } catch (const std::exception& e) {
        // Generic runtime error
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "Exception while handling client " + std::to_string(client_fd) + ": " +
                            e.what());
    } catch (...) {
        // Any other unknown error
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "Unknown exception while handling client " + std::to_string(client_fd));
    }

    // Disable further reads from this client (avoid repeated errors)
    _poll_fds[index].events &= ~POLLIN;

    // Queue an internal server error response
    respondError(client_fd, 500);

    return true; // Keep connection alive long enough to send error response
}
