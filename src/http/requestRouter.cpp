/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   requestRouter.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/12 23:13:23 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/19 10:15:21 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    requestRouter.cpp
 * @brief   Routes parsed HTTP requests to the appropriate handler.
 *
 * @details Implements the routing pipeline that maps an incoming
 *          @ref HttpRequest to a @ref HttpResponse using the active
 *          @ref Server configuration and its @ref Location blocks.
 *
 *          Workflow:
 *          1) **Directory slash redirect:** If a request targets a physical
 *             directory without a trailing slash, issue a 301 to the
 *             slash-terminated URI (see @ref redirectOnDirectorySlash).
 *          2) **Location resolution:** Find the best matching @ref Location by
 *             exact match or longest-prefix (see @ref findLocation).
 *          3) **Configured redirects:** Apply `return`-based redirects if present
 *             in the matched location (see @ref redirectOnConfigured).
 *          4) **Method validation:** Check supported/allowed methods, returning
 *             501 or 405 as appropriate (see @ref validateRequestMethod).
 *          5) **Dispatch:** Call the concrete method handler
 *             (GET/POST/DELETE) (see @ref dispatchByMethod).
 *
 *          Errors are rendered via @ref ResponseBuilder helpers. Utilities from
 *          `filesystemUtils` are used to normalize URIs and resolve physical paths.
 *
 * @ingroup request_handler
 */

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
/**
 * @brief Redirects to a slash-terminated URI when the target is a real directory.
 *
 * @details This helper enforces nginx-like behavior for directory URIs:
 *          if the request path maps to a directory but lacks a trailing slash,
 *          issue a 301 redirect to the slash-appended path.
 *
 *          Rules:
 *          - Only considers @ref Location blocks whose configured path ends with '/'.
 *          - Matches the request URI by location prefix (sans trailing slash).
 *          - Resolves a physical filesystem path with `resolvePhysicalPath` and
 *            checks it with `stat`; symlink-to-dir is treated as a directory.
 *          - Applies to any HTTP method (GET/POST/DELETE), not only GET.
 *
 * @param req   Parsed HTTP request (used for path, method, headers).
 * @param server Active server context (provides location list).
 * @param uri   Normalized request URI (usually `normalizePath(req.getPath())`).
 *
 * @return std::nullopt if no redirect is needed; otherwise a ready 301 response
 *         with `Location: <path>/`.
 *
 * @note If `req.getPath()` already ends with '/', no redirect occurs.
 * @warning Only locations defined with a trailing slash are inspected; locations
 *          configured without a trailing '/' are intentionally ignored here.
 */
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

/**
 * @brief Selects the best-matching Location for a normalized request URI.
 *
 * @details Resolution is two-phase:
 *          1) **Exact match**: return the first location whose normalized path
 *             equals `uri`.
 *          2) **Longest-prefix match**: otherwise, return the location with the
 *             longest normalized path that is a prefix of `uri`.
 *
 *          Each candidate path is normalized with `normalizePath`. Empty
 *          location paths are ignored for prefix matching.
 *
 * @param uri     Normalized request URI (e.g., from `normalizePath(req.getPath())`).
 * @param server  Active server whose location set is consulted.
 *
 * @return Pointer to the selected @ref Location, or `nullptr` if none match.
 *
 * @note Matching is purely lexical (prefix test), not filesystem-based.
 * @warning If multiple locations normalize to the same path, the first one
 *          encountered wins (implementation-defined by iteration order).
 * @complexity O(N · C) where N = number of locations and C = normalization + prefix check cost.
 */
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

/**
 * @brief Apply a location-scoped redirect, if configured.
 *
 * @details If the matched @ref Location declares a redirect (i.e. a `return` rule),
 *          this function builds and returns a redirect response using
 *          @ref ResponseBuilder::generateRedirect. The redirect is applied
 *          unconditionally for any HTTP method and happens before method
 *          validation/dispatch in the routing pipeline.
 *
 * @param request Incoming HTTP request (used to propagate version/connection metadata).
 * @param loc     The matched location to inspect for a configured redirect.
 *
 * @return A populated @ref HttpResponse when a redirect rule is present;
 *         `std::nullopt` otherwise.
 *
 * @note The target URL is taken verbatim from configuration; no normalization is
 *       performed here. Ensure the configured target is either an absolute URI or a
 *       valid absolute-path per your configuration semantics.
 *
 * @warning Choose an appropriate status code (e.g., 301/302/307/308) in the
 *          configuration to control method preservation and cacheability semantics.
 */
std::optional<HttpResponse> redirectOnConfigured(const HttpRequest& request, const Location& loc) {
    if (loc.hasRedirect()) {
        Logger::logFrom(LogLevel::INFO, "Router",
                        "Configured redirect for URI \"" + request.getPath() + "\" to \"" +
                            loc.getRedirect() + "\"");
        return ResponseBuilder::generateRedirect(loc.getReturnCode(), loc.getRedirect(), request);
    }
    return std::nullopt;
}

/**
 * @brief Validates the HTTP method against server support and location policy.
 *
 * @details Two checks in order:
 *          1) **Server support**: rejects methods not implemented by this server
 *             (currently {"GET","POST","DELETE"}) with **501 Not Implemented**.
 *          2) **Location policy**: rejects methods not allowed by the matched
 *             @ref Location with **405 Method Not Allowed**.
 *
 *          Both cases are logged and rendered via @ref ResponseBuilder::generateError.
 *
 * @param request Incoming HTTP request (used for method/path and response metadata).
 * @param loc     Matched configuration block to query method allowance.
 * @param server  Active server (used by error rendering).
 *
 * @return `std::nullopt` when the method is valid for this location; otherwise a
 *         ready error @ref HttpResponse (501 or 405).
 *
 * @note Per RFC 9110, servers **SHOULD** include an `Allow` header on 405
 *       listing permitted methods for the target resource.
 * @warning Method matching here is case-sensitive; normalize upstream if you
 *          want to accept non‑uppercase tokens.
 */
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

/**
 * @brief Dispatches a validated request to the concrete method handler.
 *
 * @details Assumes @ref validateRequestMethod has already run, so only
 *          implemented-and-allowed methods reach this point. Dispatch is a
 *          simple chain on the request method:
 *          - "GET"    → @ref handleGet
 *          - "POST"   → @ref handlePost
 *          - "DELETE" → @ref handleDelete
 *
 * @param request Parsed HTTP request.
 * @param server  Active server context.
 * @param loc     Matched @ref Location.
 *
 * @return The handler-produced @ref HttpResponse.
 *
 * @note If additional methods are added in the future, extend this dispatcher
 *       (or switch to a table-driven approach).
 */
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

/**
 * @brief Top-level router entrypoint for processing an HTTP request.
 *
 * @details This function implements the main request handling pipeline:
 *   1. **Normalize URI** — sanitize the request path.
 *   2. **Directory-slash redirect** — if a directory is requested without a trailing
 *      slash, generate a `301` redirect (see @ref redirectOnDirectorySlash).
 *   3. **Location resolution** — find the best-matching @ref Location in the
 *      server configuration (exact or longest-prefix match).
 *   4. **Configured redirect** — apply `return` directive–based redirects if present
 *      in the matched location (see @ref redirectOnConfigured).
 *   5. **Method validation** — reject unsupported (501) or disallowed (405) methods
 *      (see @ref validateRequestMethod).
 *   6. **Dispatch** — call the concrete handler for GET/POST/DELETE
 *      (see @ref dispatchByMethod).
 *
 * Errors at any stage are logged and turned into responses via @ref ResponseBuilder.
 *
 * @param request Parsed @ref HttpRequest.
 * @param server  Active @ref Server context.
 * @return Fully constructed @ref HttpResponse to send back to the client.
 *
 * @ingroup request_handler
 */
HttpResponse handleRequest(const HttpRequest& request, const Server& server) {
    // 1) Normalize the request path
    std::string uri = normalizePath(request.getPath());

    // 2) Check if this is a directory missing a trailing slash → 301 redirect
    std::optional<HttpResponse> redirectResponse = redirectOnDirectorySlash(request, server, uri);
    if (redirectResponse.has_value()) {
        return redirectResponse.value();
    }

    // 3) Find best matching Location block
    const Location* loc = findLocation(uri, server);
    if (loc == nullptr) {
        Logger::logFrom(LogLevel::WARN, "Router",
                        "No matching location for URI \"" + uri + "\" → 404");
        return ResponseBuilder::generateError(404, server, request);
    }

    // 4) Check for a configured `return` redirect on this location
    std::optional<HttpResponse> configuredRedirect = redirectOnConfigured(request, *loc);
    if (configuredRedirect.has_value()) {
        return configuredRedirect.value();
    }

    // 5) Validate that the method is implemented and allowed in this location
    std::optional<HttpResponse> validationError = validateRequestMethod(request, *loc, server);
    if (validationError.has_value()) {
        return validationError.value();
    }

    // 6) Finally, dispatch to the actual handler (GET/POST/DELETE)
    return dispatchByMethod(request, server, *loc);
}
