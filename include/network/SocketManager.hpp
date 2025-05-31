/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:51:47 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/31 12:56:43 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "core/Server.hpp"
#include "http/HttpResponse.hpp"
#include "http/handleCgi.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <optional>
#include <poll.h>
#include <queue>
#include <signal.h>
#include <unistd.h>
#include <vector>

#define TIMEOUT 300
#define HEADER_TIMEOUT_SECONDS 6
#define HEADER_MIN_LENGTH 15
#define HEADER_MAX_LENGTH 8192
#define RECV_BUFFER HEADER_MAX_LENGTH * 2
#define MAX_CLIENTS 512
#define CGI_TIMEOUT_SECONDS 45

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
    std::optional<CgiProcess> cgiProcess;
};

class SocketManager {

  public:
    SocketManager(void) = delete;
    SocketManager(const std::vector<Server>& servers);
    ~SocketManager(void);
    SocketManager(const SocketManager& other)            = delete;
    SocketManager& operator=(const SocketManager& other) = delete;

    void run();
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
    std::map<int, int> _fd_to_cgi; ///< Maps CGI stdout fds to client fds

    void setupSockets(const std::vector<Server>& servers);
    void handleNewConnection(int listen_fd);

    bool handleClientData(int client_fd, size_t index);

    void sendResponse(int client_fd, size_t index);

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
    bool receiveFromClient(int fd, size_t index);
    void respondError(int fd, int status_code);
    bool checkRequestLimits(int fd);

    bool isHeaderTimeout(int fd, time_t now);
    bool isBodyTimeout(int fd, time_t now);
    bool isSendTimeout(int fd, time_t now);
    bool isIdleTimeout(int fd, time_t now);
    void resetRequestState(int client_fd);
    void handleCgiPollEvents();
    void cleanupCgiForClient(int client_fd);
    bool handleCgiRequest(int client_fd, const HttpRequest& request, const Server& server,
                          const Location& location);
};
