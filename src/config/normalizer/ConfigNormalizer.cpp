/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigNormalizer.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 21:02:41 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/12 20:16:09 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/normalizer/ConfigNormalizer.hpp"

constexpr size_t               DEFAULT_CLIENT_MAX_BODY_SIZE = 1 * 1024 * 1024; // 1MB
const std::string              DEFAULT_ERROR_PAGE_PATH      = "/error.html";
const std::string              DEFAULT_ROOT                 = "/var/www";
const std::string              DEFAULT_INDEX                = "index.html";
const std::vector<std::string> DEFAULT_METHODS              = {"GET" /* , "HEAD" */};

void normalizeServer(Server& server) {
    // --- Normalize server-level defaults

    if (server.getClientMaxBodySize() == 0) {
        server.setClientMaxBodySize(DEFAULT_CLIENT_MAX_BODY_SIZE);
    }

    if (server.getErrorPages().empty()) {
        server.setErrorPage(500, DEFAULT_ERROR_PAGE_PATH);
        server.setErrorPage(404, DEFAULT_ERROR_PAGE_PATH);
        server.setErrorPage(403, DEFAULT_ERROR_PAGE_PATH);
        server.setErrorPage(502, DEFAULT_ERROR_PAGE_PATH);
    }

    // --- Normalize each location block
    for (Location& loc : server.getLocations()) {
        if (loc.getRoot().empty()) {
            loc.setRoot(DEFAULT_ROOT);
        }

        if (loc.getIndexFiles().empty()) {
            loc.addIndexFile(DEFAULT_INDEX);
        }

        if (!loc.hasAllowedMethods()) {
            loc.setAllowedMethods(DEFAULT_METHODS);
        }
    }
}
