/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   requestRouter.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/12 23:13:23 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/17 12:17:33 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/Location.hpp"         // for Location
#include "core/Server.hpp"           // for Server
#include "http/HttpRequest.hpp"      // for HttpRequest
#include "http/HttpResponse.hpp"     // for HttpResponse
#include "http/methodsHandler.hpp"   // for handleDelete, handleGet, handle...
#include "http/responseBuilder.hpp"  // for generateError, generateRedirect
#include "utils/Logger.hpp"          // for LogLevel, Logger
#include "utils/filesystemUtils.hpp" // for normalizePath, resolvePhysicalPath
#include <cstddef>                   // for size_t
#include <optional>                  // for optional, nullopt
#include <set>                       // for set
#include <string>                    // for allocator, operator+, char_traits
#include <sys/stat.h>                // for stat, S_ISDIR
#include <vector>                    // for vector

namespace {
std::optional<HttpResponse> redirectOnDirectorySlash(const HttpRequest& req, const Server& server,
                                                     const std::string& uri) {
    // If there's already a trailing slash in the request path, nothing to do.
    if (!req.getPath().empty() && req.getPath().back() == '/')
        return std::nullopt;

    // Try each configured Location block
    for (const Location& loc : server.getLocations()) {
        std::string locPath = normalizePath(loc.getPath());
        // We only care about locations defined with a trailing slash
        if (locPath.size() > 1 && locPath.back() == '/') {
            // Does the request URI match this location prefix (sans slash)?
            std::string prefix = locPath.substr(0, locPath.size() - 1);
            if (uri.rfind(prefix, 0) != 0)
                continue;

            // Resolve the physical FS path under this Location
            std::string fsPath = resolvePhysicalPath(req, loc);
            if (fsPath.empty())
                continue;

            struct stat st;
            if (stat(fsPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                // Found a real directory (or symlink-to-dir) — redirect any method
                std::string target = req.getPath() + "/";
                Logger::logFrom(LogLevel::INFO, "Router",
                                "Directory-slash redirect: \"" + uri + "\" → \"" + target + "\"");
                return ResponseBuilder::generateRedirect(301, target, req);
            }
        }
    }

    // No matching directory to slash-redirect
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
    if (loc.hasRedirect()) {
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
