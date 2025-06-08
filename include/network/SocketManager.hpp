/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:51:47 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/08 18:56:16 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "core/Server.hpp"       // for Server
#include "http/HttpRequest.hpp"  // for HttpRequest
#include "http/HttpResponse.hpp" // for HttpResponse
#include "http/handleCgi.hpp"    // for CgiProcess
#include <exception>             // for exception
#include <fstream>               // for basic_ifstream, ifstream
#include <map>                   // for map
#include <optional>              // for optional
#include <poll.h>                // for pollfd
#include <queue>                 // for queue
#include <string>                // for string
#include <time.h>                // for size_t, time_t
#include <vector>                // for vector

#define TIMEOUT 20
#define HEADER_TIMEOUT_SECONDS 6
#define HEADER_MIN_LENGTH 15
#define HEADER_MAX_LENGTH 8192
#define RECV_BUFFER HEADER_MAX_LENGTH * 2
#define MAX_CLIENTS 1024
#define CGI_TIMEOUT_SECONDS 45

class Location;

struct ClientInfo {
    // Core identifiers and timing
    int    client_fd;
    time_t lastRequestTime;
    time_t connectionStartTime;
    time_t lastSendAttemptTime;

    // Byte tracking
    size_t headerBytesReceived;
    size_t bodyBytesReceived;
    size_t bytes_sent;

    // Request/response flow
    bool                     headerComplete;
    std::string              requestBuffer;
    std::string              current_raw_response;
    std::vector<Server>      serversOnPort;
    std::queue<HttpRequest>  pendingRequests;
    std::queue<HttpResponse> responses;

    // CGI
    std::optional<CgiProcess> cgiProcess;
    bool                      isCgiProcessRunning;
    HttpRequest               currentCgiRequest;

    // File I/O
    std::ifstream file_stream;
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
    // Data structures
    std::vector<pollfd>                _poll_fds;
    std::map<int, std::vector<Server>> _listen_map;
    std::map<int, ClientInfo>          _client_info;
    std::map<int, int>                 _fd_to_cgi;

    // Setup & connection
    void setupSockets(const std::vector<Server>& servers);
    void handleNewConnection(int listen_fd);

    // Event handling
    bool handleClientData(int client_fd, size_t index);
    void handleCgiPollEvents();
    void sendResponse(int client_fd, size_t index);
    void handlePollError(int fd, size_t index, short revents);

    // Response helpers
    void logResponseStatus(int status, int fd);
    bool sendFileResponse(int fd, size_t index, HttpResponse& response);
    bool sendRawResponse(int fd, size_t index, HttpResponse& response);

    // Client lifecycle
    void initializeClientInfo(int client_fd, int listen_fd);
    void cleanupClientConnectionClose(int client_fd, size_t index);
    void removePollFd(size_t index);
    void cleanupClientState(int client_fd);
    void cleanupCgiForClient(int client_fd);
    void resetRequestState(int client_fd);

    // Request processing
    bool receiveFromClient(int fd, size_t index);
    bool checkRequestLimits(int fd);
    bool parseAndQueueRequests(int client_fd);
    void processPendingRequests(int client_fd);

    // Timeout checks
    bool checkClientTimeouts(int client_fd, size_t index);
    bool isHeaderTimeout(int fd, time_t now);
    bool isBodyTimeout(int fd, time_t now);
    bool isSendTimeout(int fd, time_t now);
    bool isIdleTimeout(int fd, time_t now);

    // Request routing and CGI
    bool handleCgiRequest(int client_fd, const HttpRequest& request, const Server& server,
                          const Location& location);
    bool handleRequestErrorIfAny(int fd, int code, HttpRequest& req, const Server& server);
    bool shouldSpawnCgi(const HttpRequest& req, const Location& location);

    // Error utility
    void respondError(int fd, int status_code);
};
