/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManagerRequest.cpp                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/08 14:53:38 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/08 18:58:40 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

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

bool SocketManager::receiveFromClient(int client_fd, size_t index) {
    char buffer[RECV_BUFFER];
    _client_info[client_fd].lastRequestTime = getCurrentTime();
    int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
    if (bytes == 0) {
        Logger::logFrom(LogLevel::INFO, "SocketManager",
                        "Client fd " + std::to_string(client_fd) + " disconnected.");
        cleanupClientConnectionClose(client_fd, index);
        return false;
    }
    if (bytes < 0) {
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        std::string("recv() failed: ") + std::strerror(errno));
        cleanupClientConnectionClose(client_fd, index);
        return false;
    }
    buffer[bytes] = '\0';

    std::string single_msg(buffer, bytes);
    _client_info[client_fd].requestBuffer += single_msg;
    size_t headerEndPos = _client_info[client_fd].requestBuffer.find("\r\n\r\n");

    if (headerEndPos == std::string::npos) {
        if (_client_info[client_fd].headerBytesReceived == 0) {
            _client_info[client_fd].connectionStartTime = getCurrentTime();
        }
        _client_info[client_fd].headerBytesReceived += bytes;
    } else {
        if (!_client_info[client_fd].headerComplete) {
            size_t fullHeaderSize = headerEndPos + 4;
            size_t oldBufferSize  = _client_info[client_fd].requestBuffer.size() - bytes;
            size_t headerBytesThisTime =
                std::max((ssize_t) 0, (ssize_t) (fullHeaderSize - oldBufferSize));
            _client_info[client_fd].headerBytesReceived += headerBytesThisTime;
            _client_info[client_fd].bodyBytesReceived += (bytes - headerBytesThisTime);
            _client_info[client_fd].headerComplete = true;
        } else {
            // Header already counted, this must be body
            _client_info[client_fd].bodyBytesReceived += bytes;
        }
    }

    return true;
}

static const Location* findMatchingLocation(const std::string& path, const Server& server) {
    const Location* best = nullptr;
    size_t          max  = 0;
    for (const Location& loc : server.getLocations()) {
        if (path.rfind(normalizePath(loc.getPath()), 0) == 0 &&
            normalizePath(loc.getPath()).size() > max) {
            best = &loc;
            max  = normalizePath(loc.getPath()).size();
        }
    }
    return best;
}

void SocketManager::processPendingRequests(int client_fd) {
    ClientInfo& client = _client_info[client_fd];

    // As long as there is at least one pending request AND no CGI is currently running:
    while (!client.pendingRequests.empty() && !client.isCgiProcessRunning) {
        HttpRequest   nextReq = client.pendingRequests.front();
        const Server& server  = client.serversOnPort[nextReq.getMatchedServerIndex()];

        int code = nextReq.getParseErrorCode();
        if (code != 0) {
            if (handleRequestErrorIfAny(client_fd, code, nextReq, server))
                return;
            continue;
        }

        const Location* location = findMatchingLocation(normalizePath(nextReq.getPath()), server);
        if (!location) {
            if (handleRequestErrorIfAny(client_fd, 404, nextReq, server))
                return;
            continue;
        }

        if (shouldSpawnCgi(nextReq, *location)) {
            client.currentCgiRequest   = nextReq;
            client.isCgiProcessRunning = true;
            bool ok                    = handleCgiRequest(client_fd, nextReq, server, *location);
            client.pendingRequests.pop();
            if (!ok)
                continue;
            return;
        }

        HttpResponse resp = handleRequest(nextReq, server);
        client.responses.push(resp);
        client.pendingRequests.pop();
        if (resp.isConnectionClose())
            return;
    }
}

bool SocketManager::parseAndQueueRequests(int client_fd) {
    ClientInfo& client = _client_info[client_fd];

    while (true) {
        if (checkRequestLimits(client_fd)) {
            client.requestBuffer.clear();
            resetRequestState(client_fd);
            return true;
        }

        HttpRequest request;
        int         errorCode     = 0;
        std::size_t consumedBytes = 0;

        bool ok = HttpRequestParser::parse(request, client.requestBuffer, client.serversOnPort,
                                           errorCode, consumedBytes);

        if (!ok) {
            if (errorCode == 0)
                return false; // incomplete

            request.setParseErrorCode(errorCode);
            request.printRequest();

            if (errorCode == 415 || errorCode == 411 || errorCode == 400 || errorCode == 413)
                client.requestBuffer.clear();
            else
                client.requestBuffer.erase(0, consumedBytes);

            resetRequestState(client_fd);
            client.pendingRequests.push(request);

            if (client.requestBuffer.find("\r\n\r\n") == std::string::npos)
                break;

            continue;
        }

        client.requestBuffer.erase(0, consumedBytes);
        resetRequestState(client_fd);
        client.pendingRequests.push(request);

        if (client.requestBuffer.find("\r\n\r\n") == std::string::npos)
            break;
    }

    return true;
}

void SocketManager::handleCgiPollEvents() {
    for (auto& [client_fd, client] : _client_info) {
        if (!client.cgiProcess)
            continue;

        try {
            CgiProcess&   cgi = *client.cgiProcess;
            const Server& server =
                client.serversOnPort[client.currentCgiRequest.getMatchedServerIndex()];

            if (getCurrentTime() - cgi.last_activity > CGI_TIMEOUT_SECONDS) {
                Logger::logFrom(LogLevel::WARN, "CGI",
                                "Timeout. Killing CGI process for fd: " +
                                    std::to_string(client_fd));
                client.responses.push(ResponseBuilder::generateError(504, server, {}));
                CGI::errorOnCgi(cgi);
                client.cgiProcess.reset();
                client.isCgiProcessRunning = false;
                client.currentCgiRequest   = HttpRequest();
                for (auto& pfd : _poll_fds) {
                    if (pfd.fd == client_fd) {
                        pfd.events |= POLLOUT;
                        break;
                    }
                }
                continue;
            }

            if (CGI::tryTerminateCgi(cgi)) {
                HttpResponse resp = CGI::finalizeCgi(cgi, server, client.currentCgiRequest);
                client.responses.push(resp);
                CGI::cleanupCgi(cgi);
                client.cgiProcess.reset();
                client.isCgiProcessRunning = false;
                client.currentCgiRequest   = HttpRequest();

                for (auto& pfd : _poll_fds) {
                    if (pfd.fd == client_fd) {
                        pfd.events |= POLLOUT;
                        break;
                    }
                }

                size_t idx = 0;
                for (; idx < _poll_fds.size(); ++idx) {
                    if (_poll_fds[idx].fd == client_fd)
                        break;
                }
                if (idx < _poll_fds.size())
                    processPendingRequests(client_fd);
            }
        } catch (const std::exception& e) {
            Logger::logFrom(LogLevel::ERROR, "SocketManager",
                            "Exception during CGI handling for fd " + std::to_string(client_fd) +
                                ": " + e.what());
            respondError(client_fd, 500);
            cleanupCgiForClient(client_fd);
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

bool SocketManager::handleCgiRequest(int client_fd, const HttpRequest& request,
                                     const Server& server, const Location& location) {
    ClientInfo& client = _client_info[client_fd];
    client.cgiProcess.emplace();

    int errorCode = 500;
    if (!CGI::initCgiProcess(*client.cgiProcess, request, server, location, _poll_fds, errorCode)) {
        Logger::logFrom(LogLevel::ERROR, "SocketManager",
                        "[CGI] Failed to initialize CGI process for client_fd " +
                            std::to_string(client_fd) + " with script: " + location.getPath());
        HttpResponse err = ResponseBuilder::generateError(errorCode, server, request);
        _client_info[client_fd].responses.push(err);
        client.cgiProcess.reset();
        client.isCgiProcessRunning = false;
        client.currentCgiRequest   = HttpRequest(); // clears request
        return false;                               // error response queued
    }

    return true; // handled as CGI
}

bool SocketManager::handleRequestErrorIfAny(int fd, int code, HttpRequest& req,
                                            const Server& server) {
    HttpResponse err = ResponseBuilder::generateError(code, server, req);
    _client_info[fd].responses.push(err);
    _client_info[fd].pendingRequests.pop();

    return err.isConnectionClose();
}

bool SocketManager::shouldSpawnCgi(const HttpRequest& req, const Location& location) {
    std::string resolved = location.resolveAbsolutePath(req.getPath());
    return !resolved.empty() && (req.getMethod() == "GET" || req.getMethod() == "POST") &&
           location.isCgiRequest(normalizePath(req.getPath()));
}
