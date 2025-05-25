/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:51:47 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/25 20:01:15 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    SocketManager.hpp
 * @brief   Declares the SocketManager class responsible for managing server sockets.
 *
 * @details The SocketManager sets up listening sockets, handles incoming client connections,
 * and manages client I/O using non-blocking `poll()`. It supports multiple server blocks
 * listening on different ports and performs proper cleanup on shutdown.
 *
 * @ingroup network
 */

#pragma once

#include "core/Server.hpp"
#include "http/HttpResponse.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <poll.h>
#include <queue>
#include <signal.h>
#include <unistd.h>
#include <vector>

#define TIMEOUT 15
#define HEADER_TIMEOUT_SECONDS 6
#define HEADER_MIN_LENGTH 15
#define HEADER_MAX_LENGTH 8192
#define RECV_BUFFER HEADER_MAX_LENGTH * 2
#define MAX_CLIENTS 512

/**
 * @defgroup network Networking
 * @brief Low-level socket and I/O management for HTTP servers.
 * @{
 */

struct ClientInfo {
    int                      client_fd;       // File descriptor of the client socket
    time_t                   lastRequestTime; // Last request time for timeout management
    time_t                   connectionStartTime;
    time_t                   lastSendAttemptTime;
    size_t                   headerBytesReceived;
    size_t                   bodyBytesReceived;
    bool                     headerComplete;
    size_t                   bytes_sent;
    std::string              requestBuffer;
    std::string              current_raw_response;
    bool                     keepAlive;    // Keep-alive flag
    Server                   serverConfig; // The server config the client is connected to
    std::queue<HttpResponse> responses;    // Queue of responses to be sent to the client
};

/**
 * @brief Manages all network sockets for a set of HTTP servers.
 *
 * @details The SocketManager is responsible for initializing listening sockets based on
 * configured Server objects, accepting new client connections, and processing I/O events
 * via the `poll()` syscall in a non-blocking manner. It maps file descriptors to their
 * corresponding server configurations and handles each client request accordingly.
 *
 * @ingroup network
 */
class SocketManager {

  public:
    SocketManager(void) = delete;
    /**
     * @brief Constructs a SocketManager with the given server configurations.
     *
     * @param servers A vector of Server objects representing the server configurations.
     */
    SocketManager(const std::vector<Server>& servers);
    /**
     * @brief Destructor that closes all open file descriptors.
     */
    ~SocketManager(void);
    SocketManager(const SocketManager& other)            = delete;
    SocketManager& operator=(const SocketManager& other) = delete;

    /**
     * @brief Starts the server loop, handling I/O using poll().
     *
     * @details Accepts new clients and handles data from existing ones.
     * Will exit cleanly on signal (e.g. SIGINT).
     */
    void run();
    /**
     * @brief Custom exception class for socket-related errors.
     *
     * @details Inherits from std::exception and provides a custom error message.
     */
    class SocketError : public std::exception {
      private:
        std::string _msg;

      public:
        explicit SocketError(const std::string& msg);
        virtual const char* what() const throw();
    };

  private:
    std::vector<pollfd> _poll_fds; ///< Monitored file descriptors for poll().
    std::map<int, Server>
        _listen_map; ///< Maps listening socket fds to their corresponding server configurations.
    std::map<int, ClientInfo> _client_info; /// Stores all information about each client

    /**
     * @brief Initializes all listening sockets for the provided servers.
     *
     * @param servers A vector of server configurations.
     */
    void setupSockets(const std::vector<Server>& servers);
    /**
     * @brief Accepts a new client connection and adds it to the poll list.
     *
     * @param listen_fd File descriptor of the listening socket.
     */
    void handleNewConnection(int listen_fd);
    /**
     * @brief Reads from a client socket, generates a response.
     *
     * @param client_fd File descriptor of the connected client.
     * @param index Index of the fd in the `_poll_fds` vector.
     * @return Bool indicating success if response is generated.
     */
    bool handleClientData(int client_fd, size_t index);
    /**
     * @brief Sends from a client socket, generates a response, and sends it.
     *
     * @param client_fd File descriptor of the connected client.
     * @param index Index of the fd in the `_poll_fds` vector.
     */
    void sendResponse(int client_fd, size_t index);
    /**
     * @brief Erases fds from a _client_map and _poll_fds.
     *
     * @param client_fd File descriptor of the connected client.
     * @param index Index of the fd in the `_poll_fds` vector.
     */
    void cleanupClient(int client_fd, size_t index);
    /**
     * @brief Closes client_fd and erases fds from a _client_map and _poll_fds.
     *
     * @param client_fd File descriptor of the connected client.
     * @param index Index of the fd in the `_poll_fds` vector.
     */
    void cleanupClientConnectionClose(int client_fd, size_t index);

    bool checkClientTimeouts(int client_fd, size_t index);
    /**
     * @brief Handles poll errors and cleans up the client connection.
     *
     * @param fd File descriptor of the socket with error.
     * @param index Index of the fd in the `_poll_fds` vector.
     * @param revents Events that occurred on the socket.
     */
    void handlePollError(int fd, size_t index, short revents);
    /**
     * @brief Receives data from a client socket.
     *
     * @param fd File descriptor of the connected client.
     * @param index Index of the fd in the `_poll_fds` vector.
     * @return Bool indicating success if data is successfully received.
     */
    bool receiveFromClient(int fd, size_t index);
    /**
     * @brief Sends an error response to the client.
     *
     * @details The error response is generated based on the provided HTTP status code
     * and is added to the client's response queue for sending.
     *
     * @param fd File descriptor of the connected client.
     * @param status_code HTTP status code to be sent in the error response.
     */
    void respondError(int fd, int status_code);
    /**
     * @brief Checks if the client's request exceeds predefined limits.
     *
     * @param fd File descriptor of the connected client.
     * @return Bool indicating whether the request violates any limits.
     */
    bool checkRequestLimits(int fd);

    bool isHeaderTimeout(int fd, time_t now);
    bool isBodyTimeout(int fd, time_t now);
    bool isSendTimeout(int fd, time_t now);
    bool isIdleTimeout(int fd, time_t now);
    void resetRequestState(int client_fd);
};
