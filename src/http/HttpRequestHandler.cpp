/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestHandler.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/12 23:13:23 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/26 13:52:40 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handleCgi.hpp"
#include "http/handle_delete.hpp"
#include "http/handle_get.hpp"
#include "http/handle_post.hpp"
#include <fstream>

HttpResponse handleRequest(const HttpRequest& request, const Server& server) {
    const std::string& method = request.getMethod();
    const std::string& path   = request.getPath();

    // Find matching location
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
        return ResponseBuilder::generateError(404, server, request);
    }

    const Location& location = *matched;

    // Handle HTTP redirection
    if (location.hasRedirect()) {
        return ResponseBuilder::generateRedirect(location.getReturnCode(), location.getRedirect(),
                                                 request);
    }

    static const std::set<std::string> implemented = {"GET", "POST", "DELETE"};
    if (implemented.find(method) == implemented.end()) {
        return ResponseBuilder::generateError(501, server, request);
    }

    // Method not allowed
    if (!location.isMethodAllowed(method)) {
        return ResponseBuilder::generateError(405, server, request);
    }

    /* // CGI detection
    if (location.isCgiRequest(path)) {
        return handleCgi(request, server, location);
    } */

    // Delegate based on method
    if (method == "GET") {
        return handleGet(request, server, location);
    } else if (method == "POST") {
        return handlePost(request, server, location);
    } else if (method == "DELETE") {
        return handleDelete(request, server, location);
    }

    return ResponseBuilder::generateError(500, server, request);
}
