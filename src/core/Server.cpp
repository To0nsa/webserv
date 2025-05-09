/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:51:19 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/08 16:29:55 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Server.cpp
 * @brief   Implements the Server class used for virtual host configuration.
 *
 * @details This file defines all methods of the Server class, including setters,
 *          getters, and helper logic for managing host binding, server names,
 *          error pages, body size limits, and associated location blocks.
 *          It is part of the configuration system and supports parsing and runtime use.
 *
 * @ingroup config
 */

#include "core/Server.hpp"
#include "utils/stringUtils.hpp"
#include <algorithm>
#include <string_view>

///////////////////
// --- Constructor

Server::Server()
    : _port(80) // Default HTTP port
      ,
      _host("0.0.0.0") // Default bind address
      ,
      _client_max_body_size(1 * 1024 * 1024) // 1 MiB
{
}

///////////////
// --- Setters

void Server::setPort(int port) noexcept {
    _port = port;
}

void Server::setHost(std::string_view host) noexcept {
    _host = host;
}

void Server::addServerName(std::string_view name) {
    // Append the given server name to the list of aliases.
    _server_names.emplace_back(toLower(std::string(name)));
}

void Server::setErrorPage(int code, const std::string& path) {
    // Map the given HTTP status code to a custom error page path.
    _error_pages[code] = path;
}

void Server::setClientMaxBodySize(std::size_t size) noexcept {
    _client_max_body_size = size;
}

void Server::addLocation(const Location& location) {
    // Add a new location block to the server's routing table.
    _locations.push_back(location);
}

///////////////
// --- Getters

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

std::size_t Server::getClientMaxBodySize() const noexcept {
    return _client_max_body_size;
}

const std::vector<Location>& Server::getLocations() const noexcept {
    return _locations;
}

std::vector<Location>& Server::getLocations() noexcept {
    return _locations;
}

/////////////////
// --- Utilities

bool Server::hasServerName(std::string_view name) const noexcept {
    // Check if the given name matches any of the configured server names.
    return std::any_of(_server_names.begin(), _server_names.end(),
                       [&name](const std::string& s) { return s == name; });
}
