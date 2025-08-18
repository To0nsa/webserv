/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManagerUtils.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/08 14:58:17 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/18 23:09:12 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    SocketManagerUtils.cpp
 * @brief   Utility methods for connection lifecycle management in SocketManager.
 *
 * @details
 * This file implements helper functions used internally by the
 * `SocketManager` class to manage client state, poll descriptors,
 * error handling, and cleanup routines. These utilities ensure proper
 * resource deallocation and safe connection teardown.
 *
 * Main responsibilities:
 * - Cleaning up CGI processes associated with a client (`cleanupCgiForClient`).
 * - Removing `pollfd` entries from the monitored list (`removePollFd`).
 * - Resetting client request parsing state (`resetRequestState`).
 * - Gracefully closing client connections and releasing resources
 *   (`cleanupClientState`, `cleanupClientConnectionClose`).
 * - Generating and queuing error responses (`respondError`).
 *
 * These functions are invoked by higher-level event loop and request
 * handling methods to enforce robustness, prevent resource leaks, and
 * handle exceptional conditions.
 *
 * @ingroup socket_manager
 */

#include "core/Server.hpp"           // for Server
#include "http/HttpRequest.hpp"      // for HttpRequest
#include "http/HttpResponse.hpp"     // for HttpResponse
#include "http/handleCgi.hpp"        // for CgiProcess, cleanupCgi, errorOnCgi
#include "http/responseBuilder.hpp"  // for generateError
#include "network/SocketManager.hpp" // for ClientInfo, SocketManager
#include "utils/Logger.hpp"          // for LogLevel, Logger
#include <fstream>                   // for basic_ifstream, basic_ios, basi...
#include <map>                       // for map, operator==, _Rb_tree_iterator
#include <optional>                  // for optional
#include <poll.h>                    // for pollfd
#include <queue>                     // for queue
#include <string>                    // for allocator, operator+, to_string
#include <unistd.h>                  // for close, size_t
#include <utility>                   // for pair
#include <vector>                    // for vector

/**
 * @brief Cleans up and resets CGI state for a specific client.
 *
 * @details
 * If the client identified by @p client_fd has an active CGI process,
 * this function terminates it via `CGI::cleanupCgi`, resets its state,
 * and clears the currently running CGI request.
 * If no CGI process is active, the function returns without action.
 *
 * @param client_fd The file descriptor of the client whose CGI state should be cleaned up.
 *
 * @note This function does not close the client connection itself — it only
 *       resets CGI-related state. Connection cleanup is handled separately.
 */
void SocketManager::cleanupCgiForClient(int client_fd) {
    if (!_client_info.contains(client_fd))
        return;

    ClientInfo& client = _client_info[client_fd];
    if (!client.cgiProcess)
        return;

    CGI::cleanupCgi(*client.cgiProcess);
    client.cgiProcess.reset();
    client.isCgiProcessRunning = false;
    client.currentCgiRequest   = HttpRequest();
}

/**
 * @brief Removes a file descriptor entry from the poll list.
 *
 * @details
 * This function erases the `pollfd` structure at the given index
 * from the `_poll_fds` vector, which is used by `poll()` in the
 * main event loop to monitor active sockets.
 *
 * If the index is out of range, no action is performed.
 *
 * @param index The position in `_poll_fds` corresponding to the
 *              client or listening socket to be removed.
 *
 * @note This does not close the socket itself. The caller is responsible
 *       for performing connection cleanup separately (see
 *       `cleanupClientConnectionClose`).
 */
void SocketManager::removePollFd(size_t index) {
    if (index < _poll_fds.size()) {
        _poll_fds.erase(_poll_fds.begin() + index);
    }
}

/**
 * @brief Cleans up all state associated with a client connection.
 *
 * @details
 * This function safely releases all resources tied to a client:
 * - Drains the response queue and deletes any CGI temporary files.
 * - Cleans up and terminates any active CGI process.
 * - Closes any open file streams linked to the client.
 * - Erases the client entry from `_client_info`.
 *
 * Unlike `cleanupClientConnectionClose`, this function does not close
 * the socket or modify the poll list — it only clears per-client state.
 *
 * @param client_fd The socket file descriptor of the client whose state
 *                  should be cleaned up.
 *
 * @note Call this when tearing down a client session to ensure no resource
 *       leaks (file handles, temp files, or CGI processes).
 */
void SocketManager::cleanupClientState(int client_fd) {
    auto it = _client_info.find(client_fd);
    if (it == _client_info.end())
        return;

    ClientInfo& client = it->second;

    // Clean up pending responses and unlink any CGI temp files
    while (!client.responses.empty()) {
        HttpResponse& resp = client.responses.front();
        if (resp.isCgiTempFile()) {
            CGI::unlinkWithErrorLog(resp.getCgiTempFile(), "out temp file");
        }
        client.responses.pop();
    }

    // Handle any active CGI process
    if (client.cgiProcess) {
        CGI::errorOnCgi(*client.cgiProcess);
        client.cgiProcess.reset();
        client.isCgiProcessRunning = false;
        client.currentCgiRequest   = HttpRequest();
    }

    // Close file stream if open
    if (client.file_stream.is_open()) {
        client.file_stream.close();
    }

    // Finally remove client record
    _client_info.erase(it);
}

/**
 * @brief Gracefully closes a client connection and cleans up its state.
 *
 * @details
 * This function performs a full teardown of a client connection:
 * - Removes the file descriptor from the poll list (`removePollFd`).
 * - Cleans up all per-client state (`cleanupClientState`), including
 *   pending responses, CGI processes, and open streams.
 * - Closes the actual socket file descriptor via `close()`.
 * - Logs the closure event.
 *
 * @param client_fd The socket file descriptor of the client being closed.
 * @param index     The index in `_poll_fds` corresponding to the client_fd.
 *
 * @note Use this when the connection must be terminated (e.g. client
 *       disconnect, fatal error, or `Connection: close` response).
 *       For state cleanup without closing the socket, see `cleanupClientState()`.
 */
void SocketManager::cleanupClientConnectionClose(int client_fd, size_t index) {
    removePollFd(index);
    cleanupClientState(client_fd);
    close(client_fd);
    Logger::logFrom(LogLevel::INFO, "SocketManager cleanupClientConnectionClose",
                    "Closed FD (Connection: close): " + std::to_string(client_fd));
}

/**
 * @brief Resets the parsing state of a client’s current HTTP request.
 *
 * @details
 * This function clears or resets request-related counters for a given client:
 * - If the `requestBuffer` still contains data, the connection is treated
 *   as having an incomplete header. In this case, `headerComplete` is reset
 *   to false, but `headerBytesReceived` is preserved so that header timeout
 *   tracking continues.
 * - If the buffer is empty, the function resets all parsing counters
 *   (`headerComplete`, `headerBytesReceived`, `bodyBytesReceived`).
 *
 * @param client_fd The file descriptor of the client whose request
 *                  parsing state should be reset.
 *
 * @note This is typically called after a request has been fully parsed
 *       or after an error, so the connection can be reused for the next request
 *       (in keep-alive scenarios).
 */
void SocketManager::resetRequestState(int client_fd) {
    if (!_client_info.count(client_fd))
        return;

    // If there is still data in requestBuffer, consider it a partial header
    if (!_client_info[client_fd].requestBuffer.empty()) {
        _client_info[client_fd].headerComplete = false;
        // Preserve headerBytesReceived so timeout logic remains valid
        return;
    }

    // No partial header in progress → fully reset state
    _client_info[client_fd].headerComplete      = false;
    _client_info[client_fd].headerBytesReceived = 0;
    _client_info[client_fd].bodyBytesReceived   = 0;
}

/**
 * @brief Queues an HTTP error response for a client.
 *
 * @details
 * This function generates a standardized HTTP error response with the
 * given status code (e.g. 400, 404, 500) and pushes it onto the client’s
 * response queue.
 *
 * - Uses the first server in the client’s `serversOnPort` list as a fallback
 *   context for generating the response.
 * - The request object passed to `ResponseBuilder::generateError` is empty,
 *   since the error may occur before a valid request could be parsed.
 *
 * @param fd          The client’s socket file descriptor.
 * @param status_code The HTTP status code to send (e.g. 400, 404, 500).
 *
 * @note This does not immediately send the response — it only enqueues it.
 *       The response will be transmitted later in the event loop
 *       by `sendResponse()`.
 */
void SocketManager::respondError(int fd, int status_code) {
    HttpRequest   empty;
    const Server& fallback = _client_info[fd].serversOnPort.front();
    HttpResponse  err      = ResponseBuilder::generateError(status_code, fallback, empty);
    _client_info[fd].responses.push(err);
}
