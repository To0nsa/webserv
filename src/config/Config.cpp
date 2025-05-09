/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 10:36:29 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/07 20:04:34 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Config.cpp
 * @brief   Implements the Config class for server configuration management.
 *
 * @details This file defines the member functions of the Config class,
 *          which internally stores a collection of Server instances parsed
 *          from configuration files. It provides methods to add new servers
 *          and retrieve the server list, supporting both const and mutable
 *          access. This encapsulation ensures any manipulation or iteration
 *          over the servers is done through a stable API.
 *
 * @ingroup config
 */

#include "config/Config.hpp"
#include "config/parser/ConfigParseError.hpp"

//////////////////
// --- Public API

void Config::addServer(const Server& server) {
    // Append the provided Server instance to our internal collection
    _servers.push_back(server);
}

std::vector<Server>& Config::getServers() {
    // Return the internal vector by reference so callers can modify it
    return _servers;
}

const std::vector<Server>& Config::getServers() const {
    // Return the internal vector as const to prevent modification
    return _servers;
}
