/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 10:36:29 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/18 19:48:24 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Config.cpp
 * @brief   Implements the Config aggregate for parsed servers.
 *
 * @details Minimal container methods for appending and accessing the list of
 *          @ref Server instances created during parsing.
 *
 * @ingroup config
 */

#include "config/Config.hpp"
#include "config/parser/ConfigParseError.hpp"

//=== Public API ============================================================

/**
 * @brief Appends a parsed server to the configuration.
 *
 * @param server The server instance to add.
 * @ingroup config
 */
void Config::addServer(const Server& server) {
    _servers.push_back(server); // Store by value (copy/move elision applies)
}

/**
 * @brief Returns a mutable reference to the list of servers.
 *
 * @return Vector reference for in-place modifications.
 * @ingroup config
 */
std::vector<Server>& Config::getServers() {
    return _servers;
}

/**
 * @brief Returns a const reference to the list of servers.
 *
 * @return Read-only vector reference.
 * @ingroup config
 */
const std::vector<Server>& Config::getServers() const {
    return _servers;
}
