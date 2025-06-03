/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestHandler.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/12 23:13:23 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/03 10:37:40 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/Location.hpp"
#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "http/handleCgi.hpp"
#include "http/handle_delete.hpp"
#include "http/handle_get.hpp"
#include "http/handle_post.hpp"
#include "utils/filesystemUtils.hpp"

#include <fstream>
#include <iostream>
#include <set>
#include <string>

HttpResponse handleRequest(const HttpRequest& request, const Server& server) {
    const std::string& method = request.getMethod();
    const std::string& path   = request.getPath();

    std::cout << "[Router] Handling request: method=" << method << " path=" << path << std::endl;

    // Only redirect GET from "/foo" → "/foo/".
    if (method == "GET") {
        for (const Location& loc : server.getLocations()) {
            const std::string& locPath = normalizePath(loc.getPath());
            if (locPath.length() > 1 && locPath.back() == '/' &&
                path == locPath.substr(0, locPath.size() - 1)) {
                std::cout << "[Router] 📍 Path matches redirect rule: " << path << " → " << locPath
                          << std::endl;
                return ResponseBuilder::generateRedirect(301, locPath, request);
            }
        }
    }

    // Try to find an EXACT match on loc.getPath() first ──
    const Location* matched = nullptr;
    std::string     uri     = request.getPath();
    for (const Location& loc : server.getLocations()) {
        if (uri == loc.getPath()) {
            matched = &loc;
            break;
        }
    }

    // If no exact match, do longest‐prefix ONLY for locations ending in '/' ──
    if (!matched) {
        size_t maxMatchLen = 0;
        for (const Location& loc : server.getLocations()) {
            const std::string& locPath = loc.getPath();
            // Only consider prefix if the location’s path ends with '/'
            if (!locPath.empty() && locPath.back() == '/') {
                if (uri.rfind(locPath, 0) == 0 && locPath.size() > maxMatchLen) {
                    matched     = &loc;
                    maxMatchLen = locPath.size();
                }
            }
        }
    }

    if (!matched) {
        std::cerr << "[Router] ❌ No matching location for path: " << uri << std::endl;
        return ResponseBuilder::generateError(404, server, request);
    }

    const Location& location = *matched;
    std::cout << "[Router] ✅ Matched location: " << location.getPath() << std::endl;

    // Only perform a “return …” redirect if the client is GET (or HEAD).
    // A DELETE should not trigger this redirect; it must fall through to handleDelete().
    if (method == "GET" && location.hasRedirect()) {
        std::cout << "[Router] ↪️ Redirect configured: " << location.getRedirect() << " (code "
                  << location.getReturnCode() << ")" << std::endl;
        return ResponseBuilder::generateRedirect(location.getReturnCode(), location.getRedirect(),
                                                 request);
    }

    static const std::set<std::string> implemented = {"GET", "POST", "DELETE"};
    if (implemented.find(method) == implemented.end()) {
        std::cerr << "[Router] ❌ Method not implemented: " << method << std::endl;
        return ResponseBuilder::generateError(501, server, request);
    }

    if (!location.isMethodAllowed(method)) {
        std::cerr << "[Router] ❌ Method " << method << " not allowed for this location.\n";
        return ResponseBuilder::generateError(405, server, request);
    }

    std::cout << "[Router] 🧭 Dispatching to handler for method: " << method << std::endl;

    if (method == "GET") {
        return handleGet(request, server, location);
    } else if (method == "POST") {
        return handlePost(request, server, location);
    } else if (method == "DELETE") {
        return handleDelete(request, server, location);
    }

    std::cerr << "[Router] ❌ Unknown failure dispatching method: " << method << std::endl;
    return ResponseBuilder::generateError(500, server, request);
}
