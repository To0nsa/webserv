/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 10:36:29 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/02 23:43:08 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    config.cpp
 * @brief   Implements the Config class.
 *
 * @details Stores and manages a list of Server configurations, used for virtual host resolution
 * and HTTP request routing.
 * @ingroup config
 */

#include "config/Config.hpp"

// --- Public API ---

/**
 * @brief Adds a server configuration to the list.
 *
 * @param server A fully constructed Server instance.
 */
void Config::addServer(const Server& server) {
    servers_.push_back(server);
}

/**
 * @brief Retrieves the list of configured servers.
 *
 * @return A read-only reference to the internal server list.
 */
const std::vector<Server>& Config::getServers() const {
    return servers_;
}
