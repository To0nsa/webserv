/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:51:47 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/18 22:51:45 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    SocketManager.hpp
 * @brief   Declares the SocketManager and per-client state for non-blocking I/O.
 *
 * @details The SocketManager owns the event loop (based on `poll(2)`) and
 *          multiplexes:
 *          - listening sockets (bind/listen for each configured host:port),
 *          - accepted client sockets (read → parse → route → respond),
 *          - CGI subprocess pipes (lifecycle, timeouts, finalization),
 *          - write-side backpressure and keep-alive.
 *
 *          Each connected client is tracked with a @ref ClientInfo structure
 *          that aggregates request/response buffers, timers, CGI state, and
 *          file-stream handles for zero-copy file responses.
 *
 *          The implementation guarantees:
 *          - single `poll()` over all FDs, read & write monitored together,
 *          - non-blocking I/O for sockets and CGI pipes,
 *          - accurate HTTP status codes and default error pages,
 *          - resilient behavior under stress and strict timeouts.
 *
 * @ingroup socker_mananager
 */

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

//=== Tunables & limits =======================================================

/**
 * @name Socket and protocol timeouts / limits
 * @brief Runtime guards for robustness and resource control.
 */
///@{

/** @brief Generic inactivity timeout (seconds) for body recv / idle / send. */
#define TIMEOUT 20

/** @brief Max delay to complete request headers once bytes start arriving. */
#define HEADER_TIMEOUT_SECONDS 6

/** @brief Minimal sane header size to consider a request "non-empty". */
#define HEADER_MIN_LENGTH 15

/** @brief Maximum allowed header bytes before replying 431. */
#define HEADER_MAX_LENGTH 8192

/** @brief Receive buffer size for a single `recv()` call. */
#define RECV_BUFFER HEADER_MAX_LENGTH * 2

/** @brief Hard cap on the number of concurrent tracked client FDs. */
#define MAX_CLIENTS 1024

/** @brief Max time a CGI is allowed to run without finishing (seconds). */
#define CGI_TIMEOUT_SECONDS 45
///@}

// Forward declaration to avoid header cycles.

/**
 * @brief Per-client runtime state tracked by the SocketManager.
 *
 * @details Holds identifiers, rolling counters, parsing flags, request and
 *          response queues, CGI process handle, and file streaming context.
 *          All fields are mutated only by the owning SocketManager on the
 *          poll thread.
 *
 * @ingroup network
 */
struct ClientInfo {
    //=== Core identifiers and timing ========================================

    int    client_fd;           ///< Accepted client socket FD.
    time_t lastRequestTime;     ///< Last time we received any bytes from client.
    time_t connectionStartTime; ///< First byte time for the *current* request header.
    time_t lastSendAttemptTime; ///< Last time we attempted to `send()` bytes.

    //=== Byte tracking =======================================================

    std::size_t headerBytesReceived; ///< Count of header bytes seen so far.
    std::size_t bodyBytesReceived;   ///< Count of body bytes seen so far.
    std::size_t bytes_sent;          ///< Bytes already sent from the current raw buffer.

    //=== Request/response flow ==============================================

    bool        headerComplete;       ///< True once `\r\n\r\n` found for the current request.
    std::string requestBuffer;        ///< Inbound raw buffer (may hold pipelined requests).
    std::string current_raw_response; ///< Outbound raw response header/body (when not file).

    std::vector<Server>      serversOnPort;   ///< Virtual servers sharing the same listen FD.
    std::queue<HttpRequest>  pendingRequests; ///< Parsed but not yet processed requests.
    std::queue<HttpResponse> responses;       ///< Prepared responses waiting to be sent.

    //=== CGI state ===========================================================

    std::optional<CgiProcess> cgiProcess;          ///< Active CGI process/pipe set (if any).
    bool                      isCgiProcessRunning; ///< Convenience flag while CGI is alive.
    HttpRequest               currentCgiRequest;   ///< Request currently handled by CGI.

    //=== File I/O for sendfile-like streaming ===============================

    std::ifstream file_stream; ///< Opened file for body streaming (CGI temp or static file).
};

/**
 * @brief Poll-driven socket and CGI orchestrator.
 *
 * @details
 *  Responsibilities:
 *  - Create and bind all listening sockets for configured servers.
 *  - Accept clients and maintain a single `poll()` set for all descriptors.
 *  - Receive, enforce limits, parse (supports pipelining), and route requests.
 *  - Spawn/manage CGI, enforce CGI timeout, collect output, finalize response.
 *  - Stream responses (raw / file) with keep-alive and backpressure handling.
 *  - Apply precise timeout policy (idle/header/body/send).
 *
 *  Error handling strategy:
 *  - Never block; on I/O errors or protocol violations, push an error response
 *    and cleanly close when required.
 *  - Protect the loop with wide catch blocks—FDs are closed on failure paths.
 *
 * @ingroup network
 */
class SocketManager {

  public:
    //=== Ctors / Dtor / Special members =====================================
    SocketManager(void) = delete;
    SocketManager(const std::vector<Server>& servers);
    ~SocketManager(void);
    SocketManager(const SocketManager& other)            = delete;
    SocketManager& operator=(const SocketManager& other) = delete;

    //=== Main loop ===========================================================
    void run();

    /**
     * @brief Exception type for socket-level setup/runtime failures.
     */
    class SocketError : public std::exception {
      private:
        std::string _msg;

      public:
        explicit SocketError(const std::string& msg);
        virtual const char* what() const throw();
    };

  private:
    //=== Poll & indices ======================================================

    std::vector<pollfd> _poll_fds; ///< Monitored file descriptors for poll().

    /**
     * @brief Listen FD → vhost set.
     *
     * @details For a given listen FD (host:port), all matching `Server` objects
     *          are stored here to support name-based routing.
     */
    std::map<int, std::vector<Server>> _listen_map;

    /**
     * @brief Client FD → per-client state.
     *
     * @details Contains request/response queues, timers, CGI, and file state.
     */
    std::map<int, ClientInfo> _client_info;

    /**
     * @brief CGI stdout FD → owning client FD.
     *
     * @details Used to ignore/route events on CGI pipe descriptors inside the
     *          main poll loop.
     */
    std::map<int, int> _fd_to_cgi;

    //=== Setup & connection ==================================================
    void setupSockets(const std::vector<Server>& servers);
    void handleNewConnection(int listen_fd);

    //=== Event handling ======================================================
    bool handleClientData(int client_fd, size_t index);
    void handleCgiPollEvents();
    void sendResponse(int client_fd, size_t index);
    void handlePollError(int fd, size_t index, short revents);

    //=== Response helpers ====================================================
    void logResponseStatus(int status, int fd);
    bool sendFileResponse(int fd, size_t index, HttpResponse& response);
    bool sendRawResponse(int fd, size_t index, HttpResponse& response);

    //=== Client lifecycle ====================================================
    void initializeClientInfo(int client_fd, int listen_fd);
    void cleanupClientConnectionClose(int client_fd, size_t index);
    void removePollFd(size_t index);
    void cleanupClientState(int client_fd);
    void cleanupCgiForClient(int client_fd);
    void resetRequestState(int client_fd);

    //=== Request processing ==================================================
    bool receiveFromClient(int fd, size_t index);
    bool checkRequestLimits(int fd);
    bool parseAndQueueRequests(int client_fd);
    void processPendingRequests(int client_fd);

    //=== Timeout checks ======================================================
    bool checkClientTimeouts(int client_fd, size_t index);
    bool isHeaderTimeout(int fd, time_t now);
    bool isBodyTimeout(int fd, time_t now);
    bool isSendTimeout(int fd, time_t now);
    bool isIdleTimeout(int fd, time_t now);

    //=== Request routing and CGI ============================================
    bool handleCgiRequest(int client_fd, const HttpRequest& request, const Server& server,
                          const Location& location);
    bool handleRequestErrorIfAny(int fd, int code, HttpRequest& req, const Server& server);
    bool shouldSpawnCgi(const HttpRequest& req, const Location& location);

    //=== Error utility =======================================================
    void respondError(int fd, int status_code);
};
