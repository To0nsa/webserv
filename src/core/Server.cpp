/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:51:19 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/02 21:14:08 by nlouis           ###   ########.fr       */
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
    : port_(80),                     // Default HTTP port
      host_("0.0.0.0"),              // Default bind address
      client_max_body_size_(1048576) // 1 MB
{
}

// --- Setters ---

void Server::setPort(int port) {
    port_ = port;
}

void Server::setHost(const std::string& host) {
    host_ = host;
}

void Server::addServerName(const std::string& name) {
    server_names_.push_back(name);
}

void Server::setErrorPage(int code, const std::string& path) {
    error_pages_[code] = path;
}

void Server::setClientMaxBodySize(size_t size) {
    client_max_body_size_ = size;
}

void Server::addLocation(const Location& location) {
    locations_.push_back(location);
}

// --- Getters ---

int Server::getPort() const noexcept {
    return port_;
}

const std::string& Server::getHost() const noexcept {
    return host_;
}

const std::vector<std::string>& Server::getServerNames() const noexcept {
    return server_names_;
}

const std::map<int, std::string>& Server::getErrorPages() const noexcept {
    return error_pages_;
}

size_t Server::getClientMaxBodySize() const noexcept {
    return client_max_body_size_;
}

const std::vector<Location>& Server::getLocations() const noexcept {
    return locations_;
}

bool Server::hasServerName(const std::string& name) const {
    for (const std::string& server_name : server_names_) {
        if (server_name == name)
            return true;
    }
    return false;
}