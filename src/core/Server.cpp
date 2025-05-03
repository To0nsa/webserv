/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:51:19 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/03 15:40:49 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Server.cpp
 * @brief   Implements the Server class.
 *
 * @details This file defines the member functions of the Server class, which represents
 * a virtual host configuration block. It includes logic for setting and retrieving
 * configuration parameters such as port, host, server names, error pages, client body size,
 * and per-route Location entries. It also implements host name matching via `hasServerName()`.
 *
 * @ingroup config
 */

#include "core/Server.hpp"
#include <algorithm>

// --- Constructor ---

Server::Server()
    : _port(80),                     // Default HTTP port
      _host("0.0.0.0"),              // Default bind address
      _client_max_body_size(1048576) // 1 MB
{
}

// --- Setters ---

void Server::setPort(int port) {
    _port = port;
}

void Server::setHost(const std::string& host) {
    _host = host;
}

void Server::addServerName(const std::string& name) {
    _server_names.push_back(name);
}

void Server::setErrorPage(int code, const std::string& path) {
    _error_pages[code] = path;
}

void Server::setClientMaxBodySize(size_t size) {
    _client_max_body_size = size;
}

void Server::addLocation(const Location& location) {
    _locations.push_back(location);
}

// --- Getters ---

int Server::getPort() const noexcept {
    return _port;
}

const std::string& Server::getHost() const noexcept {
    return _host;
}

const std::vector<std::string>& Server::getServerNames() const noexcept {
    return _server_names;
}

const std::map<int, std::string>& Server::getErrorPages() const noexcept {
    return _error_pages;
}

size_t Server::getClientMaxBodySize() const noexcept {
    return _client_max_body_size;
}

const std::vector<Location>& Server::getLocations() const noexcept {
    return _locations;
}

bool Server::hasServerName(const std::string& name) const {
    for (const std::string& server_name : _server_names) {
        if (server_name == name)
            return true;
    }
    return false;
}