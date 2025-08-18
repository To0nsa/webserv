/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManagerResponse.cpp                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/08 15:00:54 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/18 22:52:05 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    SocketManagerResponse.cpp
 * @brief   Implements response sending logic for SocketManager.
 *
 * @details
 * This file contains all methods of @ref SocketManager related to
 * **writing HTTP responses back to clients**:
 * - Logging response status codes (@ref logResponseStatus).
 * - Sending file-backed responses in chunks (@ref sendFileResponse).
 * - Sending in-memory/raw responses (@ref sendRawResponse).
 * - Driving the high-level send path for a client (@ref sendResponse).
 *
 * Responsibilities:
 * - Stream headers and body data over non-blocking sockets.
 * - Support both static files and CGI-generated temporary files.
 * - Handle partial writes (tracking progress via `bytes_sent`).
 * - Manage connection lifecycle (`keep-alive` vs `close`).
 * - Ensure resource cleanup (closing file streams, unlinking CGI temp files).
 *
 * This file implements the **write-path** of the event loop,
 * complementing @ref SocketManagerRequest.cpp which handles the read-path.
 *
 * @ingroup socker_mananager
 */

#include "http/HttpResponse.hpp"     // for HttpResponse
#include "http/handleCgi.hpp"        // for unlinkWithErrorLog
#include "network/SocketManager.hpp" // for ClientInfo, SocketManager
#include "utils/Logger.hpp"          // for LogLevel, Logger
#include "utils/filesystemUtils.hpp" // for getCurrentTime
#include <errno.h>                   // for errno
#include <exception>                 // for exception
#include <fstream>                   // for basic_ostream, basic_ifstream
#include <map>                       // for map, operator==, _Rb_tree_const...
#include <new>                       // for bad_alloc
#include <poll.h>                    // for pollfd, POLLOUT
#include <queue>                     // for queue
#include <sstream>                   // for basic_ostringstream
#include <string.h>                  // for strerror, size_t
#include <string>                    // for allocator, operator+, char_traits
#include <sys/socket.h>              // for send, MSG_DONTWAIT
#include <sys/types.h>               // for ssize_t
#include <utility>                   // for pair
#include <vector>                    // for vector

/**
 * @brief Logs the HTTP response status being sent to a client.
 *
 * @details
 * - Formats a message indicating the status code and target file descriptor.
 * - Logs at different severity levels depending on status code range:
 *   - `< 400`: Informational (success / redirection).
 *   - `400–499`: Warning (client error).
 *   - `>= 500`: Error (server error).
 *
 * This function is called before sending a response to provide
 * visibility into the server’s behavior and help with debugging.
 *
 * @param status The HTTP status code of the response.
 * @param fd     The client file descriptor receiving the response.
 */
void SocketManager::logResponseStatus(int status, int fd) {
    std::string message = "Sending HTTP " + std::to_string(status) + " → fd " + std::to_string(fd);
    if (status < 400)
        Logger::logFrom(LogLevel::INFO, "SocketManager sendResponse", message);
    else if (status < 500)
        Logger::logFrom(LogLevel::WARN, "SocketManager sendResponse", message);
    else
        Logger::logFrom(LogLevel::ERROR, "SocketManager sendResponse", message);
}

/**
 * @brief Sends a file-based HTTP response to a client.
 *
 * @details
 * Handles responses that stream a file (static resource or CGI temp file)
 * to the client in a **non-blocking, incremental fashion**:
 *
 * Workflow:
 * 1. **First call (headers not sent yet):**
 *    - Opens the response’s file path in binary mode.
 *    - If CGI body offset is set, seeks past CGI headers.
 *    - Builds and stores the HTTP status line and headers in
 *      `client.current_raw_response`.
 *
 * 2. **Send headers (if not fully sent yet):**
 *    - Writes remaining header bytes using `send(MSG_DONTWAIT)`.
 *    - Updates offset tracking (`bytes_sent`).
 *    - Returns early if partial write occurs (retry later).
 *
 * 3. **Send file body (chunked):**
 *    - Reads next chunk (8 KB) from the file into a buffer.
 *    - Sends it to the client socket.
 *    - Returns early if partial write occurs.
 *
 * 4. **Completion:**
 *    - On EOF (or 0 bytes read), closes file stream.
 *    - Clears `current_raw_response` and resets offset.
 *    - If response came from a CGI temp file, deletes it.
 *    - Pops the response from the queue.
 *    - Handles connection policy:
 *      - If `Connection: close` → closes client FD.
 *      - If keep-alive and no more responses → disables POLLOUT.
 *
 * Error handling:
 * - On file open failure → queues a `500 Internal Server Error`.
 * - On socket send failure → logs error, closes the connection.
 *
 * @param fd      Client socket file descriptor.
 * @param index   Index of the client FD in the poll list.
 * @param response The HTTP response object to send.
 *
 * @return `true` if more data remains to be sent,
 *         `false` if connection was closed or error occurred.
 */
bool SocketManager::sendFileResponse(int fd, size_t index, HttpResponse& response) {
    ClientInfo& client = _client_info[fd];

    // Step 1: open the file and build headers (first call)
    if (!client.file_stream.is_open()) {
        client.file_stream.open(response.getFilePath(), std::ios::binary);
        if (!client.file_stream.is_open()) {
            respondError(fd, 500);
            return false;
        }
        if (response.getCgiBodyOffset() > 0)
            client.file_stream.seekg(response.getCgiBodyOffset());

        // Build HTTP response headers
        std::ostringstream head;
        head << "HTTP/1.1 " << response.getStatusCode() << " " << response.getStatusMessage()
             << "\r\n";
        for (const auto& header : response.getHeaders())
            head << header.first << ": " << header.second << "\r\n";
        head << "\r\n";

        client.current_raw_response = head.str();
    }

    // Step 2: send HTTP headers if not finished
    std::string& raw    = client.current_raw_response;
    size_t&      offset = client.bytes_sent;
    if (offset < raw.size()) {
        ssize_t sent = send(fd, raw.c_str() + offset, raw.size() - offset, MSG_DONTWAIT);
        if (sent < 0) {
            Logger::logFrom(LogLevel::ERROR, "SocketManager",
                            "send() failed on fd " + std::to_string(fd) + ": " + strerror(errno));
            cleanupClientConnectionClose(fd, index);
            return false;
        }
        offset += sent;
        client.lastSendAttemptTime = getCurrentTime();
        if (offset < raw.size())
            return true; // partial header write → retry later
    }

    // Step 3: stream file body in chunks
    char buffer[8192];
    client.file_stream.read(buffer, sizeof(buffer));
    std::streamsize bytes_read = client.file_stream.gcount();
    if (bytes_read > 0) {
        ssize_t sent = send(fd, buffer, bytes_read, MSG_DONTWAIT);
        if (sent < 0) {
            Logger::logFrom(LogLevel::ERROR, "SocketManager",
                            "send() failed on fd " + std::to_string(fd) + ": " + strerror(errno));
            cleanupClientConnectionClose(fd, index);
            return false;
        }
        client.lastSendAttemptTime = getCurrentTime();
        return true; // still more file left
    }

    // Step 4: completion (EOF reached)
    if (client.file_stream.eof() || bytes_read == 0) {
        Logger::logFrom(LogLevel::INFO, "SocketManager",
                        "[DONE] We sent full FILE RESPONSE to fd:" + std::to_string(fd));
        client.file_stream.close();
        client.current_raw_response.clear();
        offset = 0;

        // Cleanup CGI temp file if response came from CGI
        if (response.isCgiTempFile()) {
            CGI::unlinkWithErrorLog(response.getCgiTempFile(), "out temp file");
            response.setCgiTempFile("");
        }

        // Pop response and handle connection policy
        bool shouldClose = response.isConnectionClose();
        client.responses.pop();
        if (shouldClose) {
            Logger::logFrom(LogLevel::INFO, "SocketManager",
                            "Connection: close - closing the connection");
            cleanupClientConnectionClose(fd, index);
            return false;
        } else if (client.responses.empty()) {
            Logger::logFrom(LogLevel::INFO, "SocketManager",
                            "Connection: keep-alive - keeping the connection open");
            _poll_fds[index].events &= ~POLLOUT; // disable POLLOUT until more data
        }
    }

    return true;
}

/**
 * @brief Sends an in-memory (raw) HTTP response to a client.
 *
 * @details
 * Handles responses that are already fully materialized in memory:
 * 1) On first call, serialize the @ref HttpResponse to a byte string
 *    (`toHttpString()`) and reset the per-client send offset.
 * 2) Perform non-blocking `send()` of the remaining bytes; update `bytes_sent`.
 * 3) When all bytes are sent:
 *    - Pop the response from the queue,
 *    - Clear the staging buffer and reset offset,
 *    - Honor connection policy:
 *      * `Connection: close` → close FD,
 *      * keep-alive → if no more responses, drop POLLOUT to avoid busy loops.
 *
 * Error handling:
 * - On `send()` failure, logs the error and closes the connection.
 *
 * @param fd       Client socket file descriptor.
 * @param index    Index of the client FD in the poll list.
 * @param response The response object to serialize and send.
 *
 * @return `true` if the socket remains usable (may need more sends),
 *         `false` if the connection was closed or a fatal error occurred.
 */
bool SocketManager::sendRawResponse(int fd, size_t index, HttpResponse& response) {
    ClientInfo& client = _client_info[fd];
    size_t&     offset = client.bytes_sent;

    // First invocation for this response: serialize headers + body into a single string.
    if (client.current_raw_response.empty()) {
        client.current_raw_response = response.toHttpString();
        offset                      = 0;
    }

    // Attempt to send the remaining bytes (non-blocking).
    std::string& raw = client.current_raw_response;
    if (offset < raw.size()) {
        ssize_t sent = send(fd, raw.c_str() + offset, raw.size() - offset, MSG_DONTWAIT);
        if (sent < 0) {
            Logger::logFrom(LogLevel::ERROR, "SocketManager",
                            "send() failed on fd " + std::to_string(fd) + ": " + strerror(errno));
            cleanupClientConnectionClose(fd, index);
            return false;
        }
        offset += sent;
        client.lastSendAttemptTime = getCurrentTime();
    }

    // If everything is sent, finalize and apply connection policy.
    if (offset >= raw.size()) {
        bool shouldClose = response.isConnectionClose();
        client.responses.pop();
        offset = 0;
        Logger::logFrom(LogLevel::INFO, "SocketManager",
                        "[DONE] We sent full RESPONSE to fd:" + std::to_string(fd));
        client.current_raw_response.clear();

        if (shouldClose) {
            Logger::logFrom(LogLevel::INFO, "SocketManager",
                            "Connection: close - closing the connection");
            cleanupClientConnectionClose(fd, index);
            return false;
        } else if (client.responses.empty()) {
            // No more data to write → stop polling for POLLOUT to avoid spin.
            Logger::logFrom(LogLevel::INFO, "SocketManager",
                            "Connection: keep-alive - keeping the connection open");
            _poll_fds[index].events &= ~POLLOUT;
        }
    }

    return true;
}

/**
 * @brief Dispatches sending of the next queued response for a client.
 *
 * @details
 * Retrieves the front @ref HttpResponse for the given client, logs its status
 * via @ref logResponseStatus, then delegates to the appropriate write path:
 * - File-backed response → @ref sendFileResponse (streams headers + file body).
 * - In-memory response  → @ref sendRawResponse (single serialized buffer).
 *
 * Both send paths are **non-blocking**; they may return early when the socket
 * can’t accept more bytes. Any fatal error (including allocation failure) is
 * caught here; the client connection is then closed to keep the server healthy.
 *
 * @param client_fd Client socket file descriptor.
 * @param index     Index of the client FD in the poll list.
 *
 * @note This function assumes there is at least one queued response for
 *       `client_fd`. Callers should check the queue before enabling POLLOUT.
 */
void SocketManager::sendResponse(int client_fd, size_t index) {
    try {
        // Peek the next response to send for this client.
        HttpResponse& response = _client_info[client_fd].responses.front();

        // Log status code with severity (info/warn/error).
        logResponseStatus(response.getStatusCode(), client_fd);

        // Choose write path: file-backed (streamed) vs raw (in-memory).
        if (response.isFileResponse()) {
            if (!sendFileResponse(client_fd, index, response))
                return; // connection closed or fatal error handled inside
        } else {
            if (!sendRawResponse(client_fd, index, response))
                return; // connection closed or fatal error handled inside
        }
        return; // keep connection; more data may remain for later POLLOUT
    } catch (const std::bad_alloc& e) {
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "Fatal memory allocation error while sending response to fd " +
                            std::to_string(client_fd));
    } catch (const std::exception& e) {
        // Any recoverable runtime error while sending this response.
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "Exception in sendResponse for fd " + std::to_string(client_fd) + ": " +
                            e.what());
    } catch (...) {
        // Last-resort safety net.
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "Unknown fatal error in sendResponse for fd " + std::to_string(client_fd));
    }
    // On fatal errors, close the client cleanly to avoid undefined state.
    cleanupClientConnectionClose(client_fd, index);
}
