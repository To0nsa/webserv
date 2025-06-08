/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManagerUtils.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/08 14:58:17 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/08 18:59:28 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/Server.hpp"           // for Server
#include "http/HttpRequest.hpp"      // for HttpRequest
#include "http/HttpResponse.hpp"     // for HttpResponse
#include "http/handleCgi.hpp"        // for CgiProcess, cleanupCgi, errorOnCgi
#include "http/responseBuilder.hpp"  // for generateError
#include "network/SocketManager.hpp" // for ClientInfo, SocketManager
#include "utils/Logger.hpp"          // for LogLevel, Logger
#include <algorithm>                 // for copy
#include <fstream>                   // for basic_ifstream, basic_ios, basi...
#include <map>                       // for map, operator==, _Rb_tree_iterator
#include <optional>                  // for optional
#include <poll.h>                    // for pollfd
#include <queue>                     // for queue
#include <string>                    // for allocator, operator+, to_string
#include <unistd.h>                  // for close, size_t
#include <utility>                   // for pair
#include <vector>                    // for vector

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

void SocketManager::removePollFd(size_t index) {
    if (index < _poll_fds.size()) {
        _poll_fds.erase(_poll_fds.begin() + index);
    }
}

void SocketManager::cleanupClientState(int client_fd) {
    auto it = _client_info.find(client_fd);
    if (it == _client_info.end())
        return;

    ClientInfo& client = it->second;

    while (!client.responses.empty()) {
        HttpResponse& resp = client.responses.front();
        if (resp.isCgiTempFile()) {
            CGI::unlinkWithErrorLog(resp.getCgiTempFile(), "out temp file");
        }
        client.responses.pop();
    }

    if (client.cgiProcess) {
        CGI::errorOnCgi(*client.cgiProcess);
        client.cgiProcess.reset();
        client.isCgiProcessRunning = false;
        client.currentCgiRequest   = HttpRequest();
    }

    if (client.file_stream.is_open()) {
        client.file_stream.close();
    }

    _client_info.erase(it);
}

void SocketManager::cleanupClientConnectionClose(int client_fd, size_t index) {
    removePollFd(index);
    cleanupClientState(client_fd);
    close(client_fd);
    Logger::logFrom(LogLevel::INFO, "SocketManager cleanupClientConnectionClose",
                    "Closed FD (Connection: close): " + std::to_string(client_fd));
}

void SocketManager::resetRequestState(int client_fd) {
    if (!_client_info.count(client_fd))
        return;
    // If there is still any data in requestBuffer, treat it as a partial header:
    if (!_client_info[client_fd].requestBuffer.empty()) {
        _client_info[client_fd].headerComplete = false;
        // headerBytesReceived should reflect how many bytes are already in the buffer.
        // But if we are just about to parse a brand‐new header, headerBytesReceived
        // should have already been set by receiveFromClient(...) when those bytes first arrived.
        // So here we do NOT zero it out—leave it alone so the header‐timer can still tick.
        return;
    }
    // If requestBuffer is empty, then there is no partial header in progress.
    _client_info[client_fd].headerComplete      = false;
    _client_info[client_fd].headerBytesReceived = 0;
    _client_info[client_fd].bodyBytesReceived   = 0;
}

void SocketManager::respondError(int fd, int status_code) {
    HttpRequest   empty;
    const Server& fallback = _client_info[fd].serversOnPort.front();
    HttpResponse  err      = ResponseBuilder::generateError(status_code, fallback, empty);
    _client_info[fd].responses.push(err);
}
