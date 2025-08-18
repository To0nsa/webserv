/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManagerTimeouts.cpp                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/08 15:03:19 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/18 22:52:11 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    SocketManagerTimeouts.cpp
 * @brief   Implements timeout and limit enforcement for SocketManager.
 *
 * @details
 * This file contains helper routines used by the poll-driven event loop to
 * detect stalled or abusive connections and enforce request limits:
 * - @ref isHeaderTimeout: header phase exceeded @c HEADER_TIMEOUT_SECONDS.
 * - @ref isBodyTimeout: body phase exceeded @c TIMEOUT.
 * - @ref isSendTimeout: no progress while sending for longer than @c TIMEOUT.
 * - @ref isIdleTimeout: idle connection (no header bytes) exceeded @c TIMEOUT.
 * - @ref checkClientTimeouts: orchestrates per-FD timeout checks and applies
 *   cleanup / event-mask adjustments.
 * - @ref checkRequestLimits: enforces header length cap (@c HEADER_MAX_LENGTH)
 *   and queues a 431 if exceeded.
 *
 * Responsibilities:
 * - Decide when to close connections vs. temporarily disable POLLIN.
 * - Queue appropriate HTTP error responses (e.g., 408, 431).
 * - Avoid interfering with active CGI executions.
 *
 * These functions are invoked from the main event loop to keep the server
 * responsive, fair, and resilient under slowloris-style behavior or network stalls.
 *
 * @ingroup socker_mananager
 */

#include "http/HttpResponse.hpp"     // for HttpResponse
#include "http/handleCgi.hpp"        // for CgiProcess
#include "network/SocketManager.hpp" // for ClientInfo, SocketManager, TIMEOUT
#include "utils/Logger.hpp"          // for LogLevel, Logger
#include "utils/filesystemUtils.hpp" // for getCurrentTime
#include <map>                       // for map
#include <optional>                  // for optional
#include <poll.h>                    // for pollfd, POLLIN
#include <queue>                     // for queue
#include <string>                    // for operator+, allocator, to_string
#include <time.h>                    // for time_t, size_t
#include <vector>                    // for vector

/**
 * @brief Detects header read timeout for a client connection.
 *
 * @details
 * Triggers when:
 * - No response is currently being sent (`responses` and `current_raw_response` empty),
 * - The HTTP headers are not yet complete (`headerComplete == false`) but at least
 *   one header byte has arrived (`headerBytesReceived > 0`),
 * - And the elapsed time since `connectionStartTime` exceeds
 *   `HEADER_TIMEOUT_SECONDS`.
 *
 * On timeout, queues a `408 Request Timeout` via @ref respondError and returns `true`
 * so the caller can disable further reads (POLLIN) and flush the error response.
 *
 * @param fd   Client socket file descriptor.
 * @param now  Current time snapshot used for comparison.
 * @return `true` if a header timeout was detected and an error response was queued,
 *         otherwise `false`.
 *
 * @note This does not close the connection immediately; the caller is expected to
 *       keep the socket writable to send the queued 408 response.
 */
bool SocketManager::isHeaderTimeout(int fd, time_t now) {
    ClientInfo& client = _client_info[fd];

    // No ongoing send, header not complete, some header bytes seen,
    // and the header phase exceeded the configured timeout.
    if (client.responses.empty() && client.current_raw_response.empty() && !client.headerComplete &&
        client.headerBytesReceived > 0 &&
        now - client.connectionStartTime > HEADER_TIMEOUT_SECONDS) {

        Logger::logFrom(LogLevel::WARN, "SocketManager", "Timeout on fd: " + std::to_string(fd));

        // Queue 408 so the writer path can flush it to the client.
        respondError(fd, 408);
        return true;
    }

    return false;
}

/**
 * @brief Detects request body timeout for a client connection.
 *
 * @details
 * This timeout applies when:
 * - No response is currently being sent (`responses` and `current_raw_response` empty),
 * - The HTTP headers have already been received completely (`headerComplete == true`),
 * - And the elapsed time since `connectionStartTime` exceeds @c TIMEOUT.
 *
 * This typically indicates that the client started a request but did not finish
 * sending the body in time (e.g. stalled upload or slowloris-style attack).
 *
 * On timeout, a `408 Request Timeout` is queued via @ref respondError and the function
 * returns `true`, so the caller may disable reads (`POLLIN`) and flush the error.
 *
 * @param fd   Client socket file descriptor.
 * @param now  Current server time snapshot.
 * @return `true` if a body timeout occurred and an error response was queued,
 *         otherwise `false`.
 *
 * @note The connection is not closed immediately; it is marked for sending the 408
 *       response before shutdown.
 */
bool SocketManager::isBodyTimeout(int fd, time_t now) {
    ClientInfo& client = _client_info[fd];

    // No ongoing send, header complete, and body phase exceeded TIMEOUT.
    if (client.responses.empty() && client.current_raw_response.empty() && client.headerComplete &&
        now - client.connectionStartTime > TIMEOUT) {

        Logger::logFrom(LogLevel::WARN, "SocketManager", "Timeout on fd: " + std::to_string(fd));

        respondError(fd, 408); // Queue HTTP 408 Request Timeout
        return true;
    }

    return false;
}

/**
 * @brief Detects send timeout for a client connection.
 *
 * @details
 * This timeout applies when:
 * - There are still responses waiting to be sent (`!responses.empty()`),
 * - A response is actively being transmitted (`!current_raw_response.empty()`),
 * - But no progress has been made for more than @c TIMEOUT seconds
 *   (based on `lastSendAttemptTime`).
 *
 * This usually indicates that the client has stopped reading from its socket
 * (e.g., slow or dead client), preventing the server from completing the response.
 *
 * On timeout, no error response is queued (since we are already mid-send),
 * but the function signals the caller (`true`) so that the connection can be
 * closed and cleaned up.
 *
 * @param fd   Client socket file descriptor.
 * @param now  Current server time snapshot.
 * @return `true` if a send timeout occurred and the connection should be closed,
 *         otherwise `false`.
 *
 * @note Unlike header/body timeouts, this does not queue a 408 response,
 *       because the server was already in the process of sending a response.
 */
bool SocketManager::isSendTimeout(int fd, time_t now) {
    ClientInfo& client = _client_info[fd];

    // Active send in progress but stalled for longer than TIMEOUT.
    if (!client.responses.empty() && !client.current_raw_response.empty() &&
        now - client.lastSendAttemptTime > TIMEOUT) {

        Logger::logFrom(LogLevel::WARN, "SocketManager",
                        "Send timeout on fd: " + std::to_string(fd));
        return true;
    }

    return false;
}

/**
 * @brief Detects idle timeout for a client connection.
 *
 * @details
 * This timeout applies when:
 * - The client is not running a CGI process (`cgiProcess` is empty),
 * - No response is in progress (`responses` and `current_raw_response` are empty),
 * - No headers have been fully received (`headerComplete == false`),
 * - No header bytes have been received at all (`headerBytesReceived == 0`),
 * - And the time since the last recorded client activity (`lastRequestTime`)
 *   exceeds @c TIMEOUT.
 *
 * This typically indicates an idle TCP connection where the client opened
 * a socket but never sent any meaningful data.
 *
 * On timeout, no explicit error response is queued; the caller is expected to
 * close the connection after receiving `true`.
 *
 * @param fd   Client socket file descriptor.
 * @param now  Current server time snapshot.
 * @return `true` if the connection is considered idle and should be closed,
 *         otherwise `false`.
 *
 * @note CGI processes are exempted from idle timeout handling because they may
 *       take significant time before producing output.
 */
bool SocketManager::isIdleTimeout(int fd, time_t now) {
    ClientInfo& client = _client_info[fd];

    // Skip idle timeout checks if a CGI is running.
    if (client.cgiProcess.has_value()) {
        return false;
    }

    // No response, no header progress, and idle for too long.
    if (client.responses.empty() && client.current_raw_response.empty() && !client.headerComplete &&
        client.headerBytesReceived == 0 && now - client.lastRequestTime > TIMEOUT) {

        Logger::logFrom(LogLevel::WARN, "SocketManager",
                        "Idle timeout on fd: " + std::to_string(fd));
        return true;
    }

    return false;
}

/**
 * @brief Checks and applies timeout rules for a client connection.
 *
 * @details
 * This function orchestrates all timeout checks for a client:
 * - **Idle timeout**: connection opened but no activity (`isIdleTimeout`).
 * - **Send timeout**: stalled during an ongoing response (`isSendTimeout`).
 * - **Header timeout**: headers started but not completed in time (`isHeaderTimeout`).
 * - **Body timeout**: headers done but body not finished within limit (`isBodyTimeout`).
 *
 * Behavior:
 * - If idle or send timeout occurs → the connection is closed immediately via
 *   `cleanupClientConnectionClose()`, and the function returns `false`.
 * - If header or body timeout occurs → the connection stays open long enough to
 *   send a timeout response (408). To prevent further reads, `POLLIN` is disabled
 *   for this fd, and the function returns `true`.
 * - If no timeout is triggered, returns `false`.
 *
 * @param client_fd  The socket file descriptor of the client.
 * @param index      Index of the client's pollfd entry in `_poll_fds`.
 * @return `true` if a recoverable timeout (header/body) was detected and the
 *         connection is left open for sending a response,
 *         `false` if the connection was closed or no timeout occurred.
 *
 * @note This function integrates multiple specialized timeout checks into the
 *       main poll loop, ensuring the server proactively cleans up stale or
 *       non-responsive connections.
 */
bool SocketManager::checkClientTimeouts(int client_fd, size_t index) {
    if (!_client_info.count(client_fd))
        return false;

    time_t now = getCurrentTime();

    // Hard timeouts → connection closed
    if (isIdleTimeout(client_fd, now) || isSendTimeout(client_fd, now)) {
        cleanupClientConnectionClose(client_fd, index);
        return false;
    }

    // Soft timeouts → send 408 and disable POLLIN
    if (isHeaderTimeout(client_fd, now) || isBodyTimeout(client_fd, now)) {
        _poll_fds[index].events &= ~POLLIN;
        return true;
    }

    return false;
}

/**
 * @brief Enforces request size limits for an active client connection.
 *
 * @details
 * This function checks whether the accumulated request headers for a client
 * exceed the configured maximum length (`HEADER_MAX_LENGTH`).
 *
 * Behavior:
 * - If the header is incomplete **and** the number of bytes received so far
 *   exceeds the allowed maximum, the server immediately queues a
 *   **431 Request Header Fields Too Large** error response using
 *   `respondError()`, and returns `true`.
 * - Otherwise, the request is considered within limits and processing continues.
 *
 * @param fd  The socket file descriptor of the client being checked.
 * @return `true` if the request exceeded the configured header limit
 *         (error response queued),
 *         `false` if still within allowed limits.
 *
 * @note This check is only enforced **before header completion**. Once headers
 *       are fully parsed, the limit is no longer evaluated here.
 */
bool SocketManager::checkRequestLimits(int fd) {
    ClientInfo& client = _client_info[fd];

    // Enforce header-length limit only while headers are incomplete
    if (client.headerBytesReceived > HEADER_MAX_LENGTH) {
        Logger::logFrom(LogLevel::WARN, "SocketManager",
                        "Request header too large from fd: " + std::to_string(fd));
        respondError(fd, 431); // 431: Request Header Fields Too Large
        return true;
    }

    return false;
}
