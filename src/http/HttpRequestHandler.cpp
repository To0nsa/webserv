/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestHandler.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/12 23:13:23 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/01 18:20:50 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handleCgi.hpp"
#include "http/handle_delete.hpp"
#include "http/handle_get.hpp"
#include "http/handle_post.hpp"
#include <fstream>
#include <iostream>

HttpResponse handleRequest(const HttpRequest& request, const Server& server) {
    const std::string& method = request.getMethod();
    const std::string& path   = request.getPath();

    std::cout << "[Router] Handling request: method=" << method << " path=" << path << std::endl;

    // Check for implicit redirect (e.g., "/foo" → "/foo/")
    for (const Location& loc : server.getLocations()) {
        const std::string& locPath = normalizePath(loc.getPath());
        if (locPath.length() > 1 && locPath.back() == '/' &&
            path == locPath.substr(0, locPath.size() - 1)) {
            std::cout << "[Router] 📍 Path matches redirect rule: " << path << " → " << locPath
                      << std::endl;
            return ResponseBuilder::generateRedirect(301, locPath, request);
        }
    }

    // Find best matching location block (longest prefix match)
    const Location* matched     = nullptr;
    size_t          maxMatchLen = 0;

    for (const Location& loc : server.getLocations()) {
        const std::string& locPath = normalizePath(loc.getPath());
        if (path.compare(0, locPath.size(), locPath) == 0 && locPath.size() > maxMatchLen) {
            matched     = &loc;
            maxMatchLen = locPath.size();
        }
    }

    if (!matched) {
        std::cerr << "[Router] ❌ No matching location for path: " << path << std::endl;
        return ResponseBuilder::generateError(404, server, request);
    }

    const Location& location = *matched;
    std::cout << "[Router] ✅ Matched location: " << location.getPath() << std::endl;

    // Handle configured redirection (return 301/302/etc.)
    if (location.hasRedirect()) {
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
