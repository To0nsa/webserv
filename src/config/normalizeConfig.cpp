/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   normalizeConfig.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/18 13:12:00 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 13:26:53 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    normalizeConfig.cpp
 * @brief   Implements post-parse normalization of configuration objects.
 *
 * @details Establishes default values and canonical forms for servers and
 *          locations so that runtime logic can assume non-empty, consistent
 *          configuration fields.
 *
 * @ingroup config_normalizing
 */

#include "config/normalizeConfig.hpp"

/// 1 MiB default for request body limit.
constexpr std::size_t DEFAULT_CLIENT_MAX_BODY_SIZE = 1 * 1024 * 1024;
/// Default error page path used for common error codes.
const std::string DEFAULT_ERROR_PAGE_PATH = "/error.html";
/// Default filesystem root for locations missing an explicit root.
const std::string DEFAULT_ROOT = "/var/www";
/// Default index file for the root location if none provided.
const std::string DEFAULT_INDEX = "index.html";
/// Default allowed HTTP methods when none are specified.
const std::vector<std::string> DEFAULT_METHODS = {"GET", "POST", "DELETE"};

/**
 * @brief Normalizes a single server and its locations.
 *
 * @details
 * - Ensures a default `client_max_body_size` if unset (1 MiB).
 * - Injects a default set of `error_page` mappings if none were provided.
 * - For each location:
 *   - Sets a default `root` when missing.
 *   - Adds a default `index` when the location is `/` and index list is empty.
 *   - Ensures a default set of allowed HTTP methods when unspecified.
 *
 * @param server Server to normalize (modified in place).
 *
 * @ingroup config_normalizing
 */
static void normalizeServer(Server& server) {
    // Fill default body size if not specified
    if (server.getClientMaxBodySize() == 0) {
        server.setClientMaxBodySize(DEFAULT_CLIENT_MAX_BODY_SIZE);
    }

    // Install a basic set of error pages if none provided
    if (server.getErrorPages().empty()) {
        server.setErrorPage(500, DEFAULT_ERROR_PAGE_PATH);
        server.setErrorPage(404, DEFAULT_ERROR_PAGE_PATH);
        server.setErrorPage(403, DEFAULT_ERROR_PAGE_PATH);
        server.setErrorPage(502, DEFAULT_ERROR_PAGE_PATH);
    }

    // Normalize each location
    for (Location& loc : server.getLocations()) {
        // Root path default
        if (loc.getRoot().empty()) {
            loc.setRoot(DEFAULT_ROOT);
        }

        // Provide an index for "/" if none given
        if (loc.getIndexFiles().empty() && loc.getPath() == "/") {
            loc.addIndexFile(DEFAULT_INDEX);
        }

        // Ensure at least a default method set
        if (!loc.hasAllowedMethods()) {
            loc.setAllowedMethods(DEFAULT_METHODS);
        }
    }
}

/**
 * @brief Applies normalization to all servers/locations in the config.
 *
 * @param config Parsed configuration to normalize (modified in place).
 *
 * @ingroup config_normalizing
 */
void normalizeConfig(Config& config) {
    for (Server& s : config.getServers()) {
        normalizeServer(s);
    }
}
