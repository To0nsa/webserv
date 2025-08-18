/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManagerRequest.cpp                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/08 14:53:38 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/18 22:51:58 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    SocketManagerRequest.cpp
 * @brief   Implements request handling logic for SocketManager.
 *
 * @details
 * This file contains all methods of @ref SocketManager related to
 * **client request processing**:
 * - Receiving raw data from clients (@ref receiveFromClient).
 * - Parsing HTTP requests into structured @ref HttpRequest objects
 *   (@ref parseAndQueueRequests).
 * - Executing and routing pending requests, including CGI support
 *   (@ref processPendingRequests, @ref handleCgiRequest).
 * - Managing CGI process polling, timeouts, and finalization
 *   (@ref handleCgiPollEvents).
 * - Handling request errors and generating error responses
 *   (@ref handleRequestErrorIfAny).
 *
 * It forms the **read-path of the event loop**, turning client bytes into
 * application-level requests and responses.
 *
 * @ingroup socker_mananager
 */

#include "core/Location.hpp"          // for Location
#include "core/Server.hpp"            // for Server
#include "http/HttpRequest.hpp"       // for HttpRequest
#include "http/HttpRequestParser.hpp" // for HttpRequestParser
#include "http/HttpResponse.hpp"      // for HttpResponse
#include "http/handleCgi.hpp"         // for CgiProcess, cleanupCgi, errorO...
#include "http/requestRouter.hpp"     // for handleRequest
#include "http/responseBuilder.hpp"   // for generateError
#include "network/SocketManager.hpp"  // for ClientInfo, SocketManager, CGI...
#include "utils/Logger.hpp"           // for LogLevel, Logger
#include "utils/filesystemUtils.hpp"  // for normalizePath, getCurrentTime
#include <algorithm>                  // for copy, max
#include <cstring>                    // for size_t, strerror
#include <errno.h>                    // for errno
#include <exception>                  // for exception
#include <map>                        // for map, operator==, _Rb_tree_iter...
#include <optional>                   // for optional
#include <poll.h>                     // for pollfd, POLLOUT
#include <queue>                      // for queue
#include <string>                     // for allocator, operator+, char_traits
#include <sys/socket.h>               // for recv, MSG_DONTWAIT
#include <sys/types.h>                // for ssize_t
#include <utility>                    // for pair
#include <vector>                     // for vector

/**
 * @brief Receives raw data from a client socket.
 *
 * @details
 * - Performs a non-blocking `recv()` on the given client FD.
 * - Handles connection termination cases:
 *   - `bytes == 0`: client closed the connection → cleanup and return `false`.
 *   - `bytes < 0`: read error → cleanup and return `false`.
 * - Appends received data to the client’s `requestBuffer`.
 * - Tracks request progress:
 *   - Updates header vs. body byte counters.
 *   - Marks `headerComplete` when the `\r\n\r\n` delimiter is first encountered.
 *
 * @param client_fd The client socket file descriptor.
 * @param index     Index of the client FD in the poll list (needed for cleanup).
 *
 * @return `true` if data was successfully received and buffered,
 *         `false` if the connection should be closed.
 *
 * @throws None. Errors are logged, and cleanup is performed internally.
 */
bool SocketManager::receiveFromClient(int client_fd, size_t index) {
    char buffer[RECV_BUFFER];

    // Update last activity timestamp (used for idle timeout detection)
    _client_info[client_fd].lastRequestTime = getCurrentTime();

    // Non-blocking read from the client
    int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);

    if (bytes == 0) {
        // Client performed an orderly shutdown
        Logger::logFrom(LogLevel::INFO, "SocketManager",
                        "Client fd " + std::to_string(client_fd) + " disconnected.");
        cleanupClientConnectionClose(client_fd, index);
        return false;
    }
    if (bytes < 0) {
        // Fatal read error
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        std::string("recv() failed: ") + std::strerror(errno));
        cleanupClientConnectionClose(client_fd, index);
        return false;
    }

    // Null-terminate buffer for safe string construction
    buffer[bytes] = '\0';

    // Append the new chunk to the client's request buffer
    std::string single_msg(buffer, bytes);
    _client_info[client_fd].requestBuffer += single_msg;

    // Detect end of HTTP headers ("\r\n\r\n")
    size_t headerEndPos = _client_info[client_fd].requestBuffer.find("\r\n\r\n");

    if (headerEndPos == std::string::npos) {
        // Still receiving headers
        if (_client_info[client_fd].headerBytesReceived == 0) {
            // First time seeing header data → mark connection start
            _client_info[client_fd].connectionStartTime = getCurrentTime();
        }
        _client_info[client_fd].headerBytesReceived += bytes;
    } else {
        if (!_client_info[client_fd].headerComplete) {
            // Header just completed in this recv()
            size_t fullHeaderSize = headerEndPos + 4;
            size_t oldBufferSize  = _client_info[client_fd].requestBuffer.size() - bytes;
            size_t headerBytesThisTime =
                std::max((ssize_t) 0, (ssize_t) (fullHeaderSize - oldBufferSize));

            _client_info[client_fd].headerBytesReceived += headerBytesThisTime;
            _client_info[client_fd].bodyBytesReceived += (bytes - headerBytesThisTime);
            _client_info[client_fd].headerComplete = true;
        } else {
            // Header already completed → this is pure body data
            _client_info[client_fd].bodyBytesReceived += bytes;
        }
    }

    return true;
}

/**
 * @brief Finds the best-matching Location for a given request path.
 *
 * @details
 * Iterates through all @ref Location objects configured in the given @ref Server
 * and selects the one whose `path` prefix matches the request path with the
 * **longest match length** (longest-prefix match).
 *
 * Matching logic:
 * - A location matches if `normalizePath(loc.getPath())` is a prefix of `path`.
 * - If multiple locations match, the one with the longest normalized path is chosen.
 * - If no location matches, returns `nullptr`.
 *
 * @param path   The normalized request target path (e.g. `/images/foo.jpg`).
 * @param server The server whose configured locations should be searched.
 *
 * @return Pointer to the best matching @ref Location, or `nullptr` if none match.
 */
static const Location* findMatchingLocation(const std::string& path, const Server& server) {
    const Location* best = nullptr;
    size_t          max  = 0;

    // Iterate over all configured locations for this server
    for (const Location& loc : server.getLocations()) {
        // Check if location path is a prefix of the request path
        if (path.rfind(normalizePath(loc.getPath()), 0) == 0 &&
            normalizePath(loc.getPath()).size() > max) {
            // Keep track of the longest prefix match so far
            best = &loc;
            max  = normalizePath(loc.getPath()).size();
        }
    }

    return best; // nullptr if no location matched
}

/**
 * @brief Processes all pending HTTP requests for a client.
 *
 * @details
 * Runs through the client's queued @ref HttpRequest objects and:
 * 1. Skips processing if a CGI process is currently running
 *    (only one CGI may run per client at a time).
 * 2. Checks for parse errors in the request:
 *    - If present, generates an error response via @ref handleRequestErrorIfAny.
 * 3. Resolves the request target to the best-matching @ref Location
 *    using @ref findMatchingLocation.
 *    - If no location matches, responds with 404.
 * 4. If the request must be served by a CGI:
 *    - Marks client as running CGI, initializes the process with
 *      @ref handleCgiRequest, and defers further processing.
 * 5. Otherwise, handles the request normally via @ref handleRequest and
 *    queues the @ref HttpResponse.
 *
 * The loop continues until:
 * - No more pending requests, OR
 * - A CGI request is started, OR
 * - A response requires connection close.
 *
 * @param client_fd File descriptor of the client whose requests are processed.
 */
void SocketManager::processPendingRequests(int client_fd) {
    ClientInfo& client = _client_info[client_fd];

    // Process while there are queued requests and no active CGI process
    while (!client.pendingRequests.empty() && !client.isCgiProcessRunning) {
        HttpRequest   nextReq = client.pendingRequests.front();
        const Server& server  = client.serversOnPort[nextReq.getMatchedServerIndex()];

        // Handle requests with parsing errors
        int code = nextReq.getParseErrorCode();
        if (code != 0) {
            if (handleRequestErrorIfAny(client_fd, code, nextReq, server))
                return; // stop if error requires closing connection
            continue;   // otherwise, process next pending request
        }

        // Match request path against server locations
        const Location* location = findMatchingLocation(normalizePath(nextReq.getPath()), server);
        if (!location) {
            if (handleRequestErrorIfAny(client_fd, 404, nextReq, server))
                return;
            continue;
        }

        // If request matches a CGI location → launch CGI process
        if (shouldSpawnCgi(nextReq, *location)) {
            client.currentCgiRequest   = nextReq;
            client.isCgiProcessRunning = true;
            bool ok                    = handleCgiRequest(client_fd, nextReq, server, *location);
            client.pendingRequests.pop();
            if (!ok)
                continue; // CGI init failed → continue with next request
            return;       // CGI started → stop until it finishes
        }

        // Otherwise, handle request normally and enqueue response
        HttpResponse resp = handleRequest(nextReq, server);
        client.responses.push(resp);
        client.pendingRequests.pop();

        // If response signals "Connection: close" → stop processing
        if (resp.isConnectionClose())
            return;
    }
}

/**
 * @brief Parses buffered client data into HTTP requests and queues them.
 *
 * @details
 * This function repeatedly attempts to parse complete HTTP requests
 * from a client’s `requestBuffer`:
 *
 * 1. **Check request limits:**
 *    - If headers exceed configured limits, clears buffer and pushes
 *      a `431 Request Header Fields Too Large` error.
 *
 * 2. **Parse next request:**
 *    - Uses @ref HttpRequestParser::parse to decode from the buffer.
 *    - On success: consumes parsed bytes, resets state, and queues the request.
 *    - On incomplete parse (`errorCode == 0`): returns `false` to wait for more data.
 *    - On parse error (`400`, `411`, `413`, `415`, etc.):
 *      - Marks the request with the error code.
 *      - Either clears the whole buffer (fatal errors) or erases consumed bytes.
 *      - Resets state and queues the error request.
 *
 * 3. **Loop continuation:**
 *    - If more data remains in `requestBuffer` (and headers are present),
 *      parsing continues until buffer is exhausted or incomplete.
 *
 * @param client_fd File descriptor of the client whose buffer is parsed.
 *
 * @return `true` if parsing succeeded or error requests were queued,
 *         `false` if parsing is incomplete and more data is required.
 */
bool SocketManager::parseAndQueueRequests(int client_fd) {
    ClientInfo& client = _client_info[client_fd];

    while (true) {
        // 1) enforce request size/limit rules
        if (checkRequestLimits(client_fd)) {
            client.requestBuffer.clear();
            resetRequestState(client_fd);
            return true;
        }

        HttpRequest request;
        int         errorCode     = 0;
        std::size_t consumedBytes = 0;

        // 2) attempt to parse one HTTP request from the buffer
        bool ok = HttpRequestParser::parse(request, client.requestBuffer, client.serversOnPort,
                                           errorCode, consumedBytes);

        if (!ok) {
            if (errorCode == 0)
                return false; // Incomplete request → wait for more data

            // Fatal parse error (e.g. invalid headers, bad content length)
            request.setParseErrorCode(errorCode);
            request.printRequest();

            // Clear buffer for hard errors, else drop only consumed part
            if (errorCode == 415 || errorCode == 411 || errorCode == 400 || errorCode == 413)
                client.requestBuffer.clear();
            else
                client.requestBuffer.erase(0, consumedBytes);

            // Reset parsing state and queue error request
            resetRequestState(client_fd);
            client.pendingRequests.push(request);

            // Stop if no headers left in buffer
            if (client.requestBuffer.find("\r\n\r\n") == std::string::npos)
                break;

            continue;
        }

        // Success: remove consumed bytes, reset state, queue request
        client.requestBuffer.erase(0, consumedBytes);
        resetRequestState(client_fd);
        client.pendingRequests.push(request);

        // Stop if buffer doesn’t contain another full header
        if (client.requestBuffer.find("\r\n\r\n") == std::string::npos)
            break;
    }

    return true;
}

/**
 * @brief Monitors and manages active CGI processes for all clients.
 *
 * @details
 * Iterates over all connected clients and checks if a CGI process
 * is associated with them. For each active CGI:
 *
 * 1. **Timeout check:**
 *    - If the process has been idle longer than @ref CGI_TIMEOUT_SECONDS,
 *      it is killed and a `504 Gateway Timeout` response is queued.
 *
 * 2. **Completion check:**
 *    - If @ref CGI::tryTerminateCgi indicates the process is done,
 *      finalize output with @ref CGI::finalizeCgi, push response,
 *      cleanup process state, and re-enable `POLLOUT` to send data.
 *    - After cleanup, if more requests remain queued for the client,
 *      continue processing them with @ref processPendingRequests.
 *
 * 3. **Exception safety:**
 *    - If any error occurs during CGI handling, logs the error,
 *      queues a `500 Internal Server Error`, and cleans up CGI state.
 *
 * Integration:
 * - This method is called once per poll loop iteration before
 *   handling socket-level events.
 * - It ensures CGI subprocesses do not block the server’s main loop.
 *
 * @see processPendingRequests
 */
void SocketManager::handleCgiPollEvents() {
    for (auto& [client_fd, client] : _client_info) {
        if (!client.cgiProcess)
            continue; // Skip clients without an active CGI

        try {
            CgiProcess&   cgi = *client.cgiProcess;
            const Server& server =
                client.serversOnPort[client.currentCgiRequest.getMatchedServerIndex()];

            // 1. Timeout check
            if (getCurrentTime() - cgi.last_activity > CGI_TIMEOUT_SECONDS) {
                Logger::logFrom(LogLevel::WARN, "CGI",
                                "Timeout. Killing CGI process for fd: " +
                                    std::to_string(client_fd));
                client.responses.push(ResponseBuilder::generateError(504, server, {}));
                CGI::errorOnCgi(cgi);      // Kill process with error
                client.cgiProcess.reset(); // Drop process handle
                client.isCgiProcessRunning = false;
                client.currentCgiRequest   = HttpRequest();

                // Ensure POLLOUT so timeout response gets sent
                for (auto& pfd : _poll_fds) {
                    if (pfd.fd == client_fd) {
                        pfd.events |= POLLOUT;
                        break;
                    }
                }
                continue;
            }

            // 2. Completion check
            if (CGI::tryTerminateCgi(cgi)) {
                HttpResponse resp = CGI::finalizeCgi(cgi, server, client.currentCgiRequest);
                client.responses.push(resp);
                CGI::cleanupCgi(cgi); // Free temp files/resources
                client.cgiProcess.reset();
                client.isCgiProcessRunning = false;
                client.currentCgiRequest   = HttpRequest();

                // Mark socket ready for sending CGI response
                for (auto& pfd : _poll_fds) {
                    if (pfd.fd == client_fd) {
                        pfd.events |= POLLOUT;
                        break;
                    }
                }

                // Resume processing queued requests (pipelined)
                size_t idx = 0;
                for (; idx < _poll_fds.size(); ++idx) {
                    if (_poll_fds[idx].fd == client_fd)
                        break;
                }
                if (idx < _poll_fds.size())
                    processPendingRequests(client_fd);
            }
        } catch (const std::exception& e) {
            // 3. Exception handling: fail safe
            Logger::logFrom(LogLevel::ERROR, "SocketManager",
                            "Exception during CGI handling for fd " + std::to_string(client_fd) +
                                ": " + e.what());
            respondError(client_fd, 500);   // Queue internal error
            cleanupCgiForClient(client_fd); // Force cleanup

            // Mark socket for POLLOUT to flush error response
            for (auto& pfd : _poll_fds) {
                if (pfd.fd == client_fd) {
                    pfd.events |= POLLOUT;
                    break;
                }
            }
        }
    }
}

bool hasFullChunkedBody(const std::string& buffer, size_t bodyStart) {
    size_t end = buffer.find("0\r\n\r\n", bodyStart);
    return end != std::string::npos;
}

/**
 * @brief Initializes and starts a CGI process for a client request.
 *
 * @details
 * - Allocates a new @ref CgiProcess in the client state.
 * - Attempts to initialize the CGI environment via @ref CGI::initCgiProcess.
 *   - On success: leaves the process active and returns `true`.
 *   - On failure:
 *     - Logs an error with details.
 *     - Queues an error @ref HttpResponse (e.g. `500`, or errorCode returned).
 *     - Cleans up CGI state and clears the current request.
 *     - Returns `false`.
 *
 * @param client_fd The client file descriptor.
 * @param request   The HTTP request to be executed by CGI.
 * @param server    The server context for this request.
 * @param location  The location context where CGI execution is configured.
 *
 * @return `true` if CGI initialization succeeded,
 *         `false` if an error response was queued instead.
 *
 * @see handleCgiPollEvents, processPendingRequests
 */
bool SocketManager::handleCgiRequest(int client_fd, const HttpRequest& request,
                                     const Server& server, const Location& location) {
    ClientInfo& client = _client_info[client_fd];
    client.cgiProcess.emplace(); // Allocate a new CGI process slot

    int errorCode = 500; // Default fallback error code
    if (!CGI::initCgiProcess(*client.cgiProcess, request, server, location, _poll_fds, errorCode)) {
        // Failed to spawn CGI → log and queue error response
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "[CGI] Failed to initialize CGI process for client_fd " +
                            std::to_string(client_fd) + " with script: " + location.getPath());

        HttpResponse err = ResponseBuilder::generateError(errorCode, server, request);
        _client_info[client_fd].responses.push(err);

        // Reset CGI state so client can recover
        client.cgiProcess.reset();
        client.isCgiProcessRunning = false;
        client.currentCgiRequest   = HttpRequest();

        return false; // Error response queued instead of running CGI
    }

    return true; // CGI successfully started, will be managed by handleCgiPollEvents()
}

/**
 * @brief Handles an HTTP request error by generating and queuing an error response.
 *
 * @details
 * - Builds an error @ref HttpResponse using @ref ResponseBuilder::generateError.
 * - Pushes the response into the client’s response queue.
 * - Removes the faulty request from the pending request queue.
 * - Returns whether the response requires closing the connection (e.g. HTTP/1.0 without
 * keep-alive).
 *
 * @param fd     File descriptor of the client connection.
 * @param code   HTTP status code to return (e.g. 400, 404, 413).
 * @param req    The offending HTTP request (may be partially parsed).
 * @param server The server context used for error page resolution.
 *
 * @return `true` if the error response requires closing the connection,
 *         `false` otherwise (keep-alive).
 */
bool SocketManager::handleRequestErrorIfAny(int fd, int code, HttpRequest& req,
                                            const Server& server) {
    // Generate error response for this request
    HttpResponse err = ResponseBuilder::generateError(code, server, req);

    // Queue the response to be sent back to the client
    _client_info[fd].responses.push(err);

    // Drop the faulty request from the pending queue
    _client_info[fd].pendingRequests.pop();

    // Decide if the connection must be closed after sending this response
    return err.isConnectionClose();
}

/**
 * @brief Determines whether a request should be handled by a CGI process.
 *
 * @details
 * A request qualifies for CGI execution if:
 * - The path can be resolved to a valid absolute file path (`resolveAbsolutePath` not empty).
 * - The HTTP method is either `GET` or `POST` (other methods are not CGI-eligible).
 * - The resolved path matches a CGI-enabled location (`isCgiRequest`).
 *
 * If all conditions are met, returns `true` so the request is delegated
 * to @ref handleCgiRequest. Otherwise, it should be served as a normal static/dynamic response.
 *
 * @param req       The incoming HTTP request to evaluate.
 * @param location  The matched @ref Location context for the request path.
 *
 * @return `true` if the request should spawn a CGI process,
 *         `false` if it should be handled normally.
 */
bool SocketManager::shouldSpawnCgi(const HttpRequest& req, const Location& location) {
    // Resolve absolute filesystem path of the requested resource
    std::string resolved = location.resolveAbsolutePath(req.getPath());

    // Only GET/POST methods are supported for CGI execution,
    // and only if the path belongs to a CGI-enabled location.
    return !resolved.empty() && (req.getMethod() == "GET" || req.getMethod() == "POST") &&
           location.isCgiRequest(normalizePath(req.getPath()));
}
