/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server_utils.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/02 20:29:58 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/02 22:03:05 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    server_utils.hpp
 * @brief   Provides utility functions for selecting the correct Server instance.
 *
 * @details Contains logic for virtual host resolution based on the request's
 * Host header and destination port. These functions are used during HTTP
 * request routing to determine which configured server block should handle
 * the incoming connection.
 *
 * @ingroup config
 */

#pragma once

#include "Server.hpp"
#include <string>
#include <vector>

/**
 * @brief Selects the best matching server for a given port and Host header.
 *
 * @param servers List of all servers loaded from config.
 * @param port The port on which the connection was accepted.
 * @param host_name The Host header value from the request (e.g. "localhost").
 * @return Reference to the selected Server.
 */
const Server& findMatchingServer(const std::vector<Server>& servers, int port,
                                 const std::string& host_name);
