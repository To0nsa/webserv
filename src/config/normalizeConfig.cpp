/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   normalizeConfig.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 21:02:41 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/20 22:37:46 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/normalizeConfig.hpp"

constexpr size_t               DEFAULT_CLIENT_MAX_BODY_SIZE = 1 * 1024 * 1024;
const std::string              DEFAULT_ERROR_PAGE_PATH      = "/error.html";
const std::string              DEFAULT_ROOT                 = "/var/www";
const std::string              DEFAULT_INDEX                = "index.html";
const std::vector<std::string> DEFAULT_METHODS              = {"GET", "POST", "DELETE"};

static void normalizeServer(Server& server) {
    if (server.getClientMaxBodySize() == 0) {
        server.setClientMaxBodySize(DEFAULT_CLIENT_MAX_BODY_SIZE);
    }

    if (server.getErrorPages().empty()) {
        server.setErrorPage(500, DEFAULT_ERROR_PAGE_PATH);
        server.setErrorPage(404, DEFAULT_ERROR_PAGE_PATH);
        server.setErrorPage(403, DEFAULT_ERROR_PAGE_PATH);
        server.setErrorPage(502, DEFAULT_ERROR_PAGE_PATH);
    }

    for (Location& loc : server.getLocations()) {
        if (loc.getRoot().empty()) {
            loc.setRoot(DEFAULT_ROOT);
        }

        if (loc.getIndexFiles().empty() && loc.getPath() == "/") {
            loc.addIndexFile(DEFAULT_INDEX);
        }

        if (!loc.hasAllowedMethods()) {
            loc.setAllowedMethods(DEFAULT_METHODS);
        }
    }
}

void normalizeConfig(Config& config) {
    for (Server& s : config.getServers()) {
        normalizeServer(s);
    }
}
