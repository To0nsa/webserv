/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server_utils.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/02 20:30:41 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/02 22:10:11 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/server_utils.hpp"
#include <stdexcept>

/**
 * @brief Selects the best matching Server for a given port and Host header.
 *
 * @details This function implements name-based virtual host resolution.
 * It searches for a server block that matches both the listening port
 * and the requested `Host:` header. If no exact match is found, it falls
 * back to the first server configured on the target port.
 *
 * @param servers   All parsed Server instances.
 * @param port      The port the connection was accepted on.
 * @param host_name The Host header from the HTTP request (e.g. "example.com").
 * @return A reference to the matching Server instance.
 *
 * @throws std::runtime_error If no Server listens on the given port.
 *
 * @warning When using this function in unit tests, avoid passing string literals
 * like `"example.com"` directly to `host_name` — this may trigger
 * `-Werror=dangling-reference` with strict compilers. Use a `std::string` variable
 * instead. This is not an issue in production code, where the Host header is
 * always stored in a string variable.
 */
const Server& findMatchingServer(const std::vector<Server>& servers, int port,
                                 const std::string& host_name) {
    // Pass 1: Look for a server that matches both port and Host name
    for (const Server& server : servers) {
        if (server.getPort() == port && server.hasServerName(host_name)) {
            return server; // Exact match
        }
    }

    // Pass 2: No name match — fall back to the first server that listens on the same port
    for (const Server& server : servers) {
        if (server.getPort() == port) {
            return server; // Fallback default server
        }
    }

    // No server found on the target port — likely config error
    throw std::runtime_error("No matching server found for port " + std::to_string(port));
}
