/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManagerResponse.cpp                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/08 15:00:54 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/08 18:59:11 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

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

void SocketManager::logResponseStatus(int status, int fd) {
    std::string message = "Sending HTTP " + std::to_string(status) + " → fd " + std::to_string(fd);
    if (status < 400)
        Logger::logFrom(LogLevel::INFO, "SocketManager sendResponse", message);
    else if (status < 500)
        Logger::logFrom(LogLevel::WARN, "SocketManager sendResponse", message);
    else
        Logger::logFrom(LogLevel::ERROR, "SocketManager sendResponse", message);
}

bool SocketManager::sendFileResponse(int fd, size_t index, HttpResponse& response) {
    ClientInfo& client = _client_info[fd];

    if (!client.file_stream.is_open()) {
        client.file_stream.open(response.getFilePath(), std::ios::binary);
        if (!client.file_stream.is_open()) {
            respondError(fd, 500);
            return false;
        }
        if (response.getCgiBodyOffset() > 0)
            client.file_stream.seekg(response.getCgiBodyOffset());

        std::ostringstream head;
        head << "HTTP/1.1 " << response.getStatusCode() << " " << response.getStatusMessage()
             << "\r\n";
        for (const auto& header : response.getHeaders())
            head << header.first << ": " << header.second << "\r\n";
        head << "\r\n";
        client.current_raw_response = head.str();
    }

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
            return true;
    }

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
        return true;
    }

    if (client.file_stream.eof() || bytes_read == 0) {
        Logger::logFrom(LogLevel::INFO, "SocketManager",
                        "[DONE] We sent full FILE RESPONSE to fd:" + std::to_string(fd));
        client.file_stream.close();
        client.current_raw_response.clear();
        offset = 0;

        if (response.isCgiTempFile()) {
            CGI::unlinkWithErrorLog(response.getCgiTempFile(), "out temp file");
            response.setCgiTempFile("");
        }
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
            _poll_fds[index].events &= ~POLLOUT; // Disable POLLOUT for keep-alive
        }
    }

    return true;
}

bool SocketManager::sendRawResponse(int fd, size_t index, HttpResponse& response) {
    ClientInfo& client = _client_info[fd];
    size_t&     offset = client.bytes_sent;

    if (client.current_raw_response.empty()) {
        client.current_raw_response = response.toHttpString();
        offset                      = 0;
    }

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
            Logger::logFrom(LogLevel::INFO, "SocketManager",
                            "Connection: keep-alive - keeping the connection open");
            _poll_fds[index].events &= ~POLLOUT; // Disable POLLOUT for keep-alive
        }
    }

    return true;
}

void SocketManager::sendResponse(int client_fd, size_t index) {
    try {
        HttpResponse& response = _client_info[client_fd].responses.front();

        logResponseStatus(response.getStatusCode(), client_fd);

        if (response.isFileResponse()) {
            if (!sendFileResponse(client_fd, index, response))
                return;
        } else {
            if (!sendRawResponse(client_fd, index, response))
                return;
        }
        return;
    } catch (const std::bad_alloc& e) {
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "Fatal memory allocation error while sending response to fd " +
                            std::to_string(client_fd));
    } catch (const std::exception& e) {
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "Exception in sendResponse for fd " + std::to_string(client_fd) + ": " +
                            e.what());
    } catch (...) {
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "Unknown fatal error in sendResponse for fd " + std::to_string(client_fd));
    }
    cleanupClientConnectionClose(client_fd, index);
}
