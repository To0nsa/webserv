/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   requestRouter.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/12 23:13:23 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/09 00:18:35 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/Location.hpp"
#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include "http/handleCgi.hpp"
#include "http/methodsHandler.hpp"
#include "http/responseBuilder.hpp"
#include "utils/Logger.hpp"
#include "utils/filesystemUtils.hpp"

#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <string>

namespace {
// 1) Redirect “/foo” → “/foo/” when GET
std::optional<HttpResponse> redirectOnDirectorySlash(const HttpRequest& request,
                                                     const Server& server, const std::string& uri) {
    const std::string& method = request.getMethod();
    if (method != "GET") {
        return std::nullopt;
    }

    for (const Location& loc : server.getLocations()) {
        std::string locPath = normalizePath(loc.getPath());
        if (locPath.size() > 1 && locPath.back() == '/') {
            std::string withoutSlash = locPath.substr(0, locPath.size() - 1);
            if (uri == withoutSlash) {
                Logger::logFrom(LogLevel::INFO, "Router",
                                "Directory-slash redirect: \"" + uri + "\" → \"" + locPath + "\"");
                return ResponseBuilder::generateRedirect(301, locPath, request);
            }
        }
    }
    return std::nullopt;
}

// 2) Find exact‐match or longest‐prefix location
const Location* findLocation(const std::string& uri, const Server& server) {
    // Exact match
    for (const Location& loc : server.getLocations()) {
        std::string locPath = normalizePath(loc.getPath());
        if (uri == locPath) {
            return &loc;
        }
    }
    // Longest‐prefix
    const Location* best    = nullptr;
    std::size_t     bestLen = 0;
    for (const Location& loc : server.getLocations()) {
        std::string locPath = normalizePath(loc.getPath());
        if (locPath.empty()) {
            continue;
        }
        if (uri.rfind(locPath, 0) == 0 && locPath.size() > bestLen) {
            best    = &loc;
            bestLen = locPath.size();
        }
    }
    return best;
}

// 3) Handle configured “return” redirects on GET
std::optional<HttpResponse> redirectOnConfigured(const HttpRequest& request, const Location& loc) {
    const std::string& method = request.getMethod();
    if (method == "GET" && loc.hasRedirect()) {
        Logger::logFrom(LogLevel::INFO, "Router",
                        "Configured redirect for URI \"" + request.getPath() + "\" to \"" +
                            loc.getRedirect() + "\"");
        return ResponseBuilder::generateRedirect(loc.getReturnCode(), loc.getRedirect(), request);
    }
    return std::nullopt;
}

// 4) Check for un-implemented / not-allowed methods
std::optional<HttpResponse> validateRequestMethod(const HttpRequest& request, const Location& loc,
                                                  const Server& server) {
    static const std::set<std::string> supported = {"GET", "POST", "DELETE"};
    const std::string&                 method    = request.getMethod();

    if (supported.count(method) == 0) {
        Logger::logFrom(LogLevel::WARN, "Router",
                        "Method \"" + method + "\" not implemented for URI \"" + request.getPath() +
                            "\"");
        return ResponseBuilder::generateError(501, server, request);
    }

    if (!loc.isMethodAllowed(method)) {
        Logger::logFrom(LogLevel::WARN, "Router",
                        "Method \"" + method + "\" not allowed on location \"" + loc.getPath() +
                            "\"");
        return ResponseBuilder::generateError(405, server, request);
    }

    return std::nullopt;
}

// 5) Dispatch to the specific handler
HttpResponse dispatchByMethod(const HttpRequest& request, const Server& server,
                              const Location& loc) {
    const std::string& method = request.getMethod();
    if (method == "GET") {
        return handleGet(request, server, loc);
    }
    if (method == "POST") {
        return handlePost(request, server, loc);
    }
    // Only DELETE remains
    return handleDelete(request, server, loc);
}
} // anonymous namespace

HttpResponse handleRequest(const HttpRequest& request, const Server& server) {
    std::string uri = normalizePath(request.getPath());

    std::optional<HttpResponse> redirectResponse = redirectOnDirectorySlash(request, server, uri);
    if (redirectResponse.has_value()) {
        return redirectResponse.value();
    }

    const Location* loc = findLocation(uri, server);
    if (loc == nullptr) {
        Logger::logFrom(LogLevel::WARN, "Router",
                        "No matching location for URI \"" + uri + "\" → 404");
        return ResponseBuilder::generateError(404, server, request);
    }

    std::optional<HttpResponse> configuredRedirect = redirectOnConfigured(request, *loc);
    if (configuredRedirect.has_value()) {
        return configuredRedirect.value();
    }

    std::optional<HttpResponse> validationError = validateRequestMethod(request, *loc, server);
    if (validationError.has_value()) {
        return validationError.value();
    }

    return dispatchByMethod(request, server, *loc);
}
