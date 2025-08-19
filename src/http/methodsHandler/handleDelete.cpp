/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleDelete.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/21 15:06:07 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/19 10:23:19 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    methodsHandler.hpp
 * @brief   Declares handlers for supported HTTP request methods.
 *
 * @details This header exposes the top-level functions used by the
 *          request router to process method-specific logic:
 *          - @ref handleGet: serve static files, run CGI, or generate
 *            autoindex listings.
 *          - @ref handlePost: handle client uploads (raw body,
 *            URL-encoded forms, multipart forms).
 *          - @ref handleDelete: remove existing resources from the
 *            server filesystem.
 *
 *          Additionally, it provides:
 *          - @ref generateAutoindex: build a directory listing page
 *            when autoindexing is enabled.
 *          - @ref handleMultipartForm: internal helper for parsing
 *            multipart form-data uploads.
 *
 *          These functions are invoked by the router after method
 *          validation (see @ref requestRouter.cpp).
 *
 * @ingroup request_handler
 */

#include "http/HttpRequest.hpp"      // for HttpRequest
#include "http/HttpResponse.hpp"     // for HttpResponse
#include "http/responseBuilder.hpp"  // for generateError, generateSuccess
#include "utils/Logger.hpp"          // for LogLevel, Logger
#include "utils/filesystemUtils.hpp" // for isSymlink, resolvePhysicalPath
#include "utils/htmlUtils.hpp"       // for htmlEscape
#include "utils/urlUtils.hpp"        // for extractFilenameFromUri
#include <errno.h>                   // for EACCES, ENOENT, EPERM
#include <filesystem>                // for remove, path
#include <sstream>                   // for basic_ostream, operator<<, basi...
#include <string>                    // for allocator, operator+, char_traits
#include <sys/stat.h>                // for stat, S_ISDIR, S_ISREG
#include <system_error>              // for error_code
class Location;
class Server;

namespace {

/**
 * @brief Attempts to delete a file safely, producing an error response on failure.
 *
 * @details This helper encapsulates the low-level checks and filesystem
 *          operations for handling HTTP `DELETE` requests. It verifies:
 *          - Existence of the target (returns 404 if not found).
 *          - That the target is **not** a directory or special file (returns 403).
 *          - That the target is a regular file (otherwise rejected with 403).
 *          - Filesystem deletion via `std::filesystem::remove`, with
 *            detailed error mapping:
 *              - EACCES / EPERM → 403 Forbidden
 *              - ENOENT (file vanished) → 404 Not Found
 *              - Other errors → 500 Internal Server Error
 *
 *          On success, the file is removed and the function returns `true`.
 *          On failure, `outError` is populated with an appropriate
 *          @ref HttpResponse.
 *
 * @param filepath Full path to the file to delete.
 * @param req      Original HTTP request (used in error responses).
 * @param server   Active server context (used in error responses).
 * @param outError Populated with a generated error response if deletion fails.
 *
 * @return `true` if the file was successfully unlinked, `false` otherwise.
 *
 * @ingroup request_handler
 */
bool unlinkFile(const std::string& filepath, const HttpRequest& req, const Server& server,
                HttpResponse& outError) {
    struct stat st;

    // 1) Stat the file: reject if not found
    if (stat(filepath.c_str(), &st) != 0) {
        Logger::logFrom(LogLevel::WARN, "Delete Handler",
                        "File not found → rejecting DELETE: " + filepath);
        outError = ResponseBuilder::generateError(404, server, req);
        return false;
    }

    // 2) Reject directories outright
    if (S_ISDIR(st.st_mode)) {
        Logger::logFrom(LogLevel::WARN, "Delete Handler",
                        "Target is a directory → rejecting DELETE: " + filepath);
        outError = ResponseBuilder::generateError(403, server, req);
        return false;
    }

    // 3) Reject non-regular files (devices, sockets, etc.)
    if (!S_ISREG(st.st_mode)) {
        Logger::logFrom(LogLevel::WARN, "Delete Handler",
                        "Target is not a regular file → rejecting DELETE: " + filepath);
        outError = ResponseBuilder::generateError(403, server, req);
        return false;
    }

    // 4) Attempt deletion with detailed error handling
    std::error_code ec;
    if (!std::filesystem::remove(filepath, ec)) {
        if (ec.value() == EACCES || ec.value() == EPERM) {
            Logger::logFrom(LogLevel::WARN, "Delete Handler",
                            "Permission denied when deleting: " + filepath + " (" + ec.message() +
                                ")");
            outError = ResponseBuilder::generateError(403, server, req);
        } else if (ec.value() == ENOENT) {
            Logger::logFrom(LogLevel::WARN, "Delete Handler",
                            "File disappeared before deletion: " + filepath);
            outError = ResponseBuilder::generateError(404, server, req);
        } else {
            Logger::logFrom(LogLevel::WARN, "Delete Handler",
                            "Unexpected error deleting file: " + filepath + " (" + ec.message() +
                                ")");
            outError = ResponseBuilder::generateError(500, server, req);
        }
        return false;
    }

    // Success: file removed
    return true;
}

/**
 * @brief Generates a simple HTML confirmation page for a deleted file.
 *
 * @details Builds a minimal HTML5 document containing a title and message
 *          confirming that the requested file has been successfully deleted.
 *          The filename is HTML-escaped before insertion to prevent XSS.
 *
 * @param filename Name of the deleted file (not a path). Will be sanitized
 *                 via @ref htmlEscape before being injected into the HTML.
 *
 * @return A fully-formed HTML string suitable as the body of a 200 OK response.
 *
 * @ingroup request_handler
 */
std::string generateDeleteHtml(const std::string& filename) {
    std::stringstream html;
    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Deleted: )"
         << htmlEscape(filename) << R"(</title>
</head>
<body>
  <div class="container">
    <h1>File )"
         << htmlEscape(filename) << R"( deleted.</h1>
    <p>The requested file has been successfully removed.</p>
  </div>
</body>
</html>)";
    return html.str();
}

} // namespace

/**
 * @brief Handles an HTTP DELETE request for a given resource.
 *
 * @details This function enforces server security rules and attempts
 *          to remove the requested file if permitted:
 *          1. Resolve the request path to a physical filesystem path.
 *          2. Reject empty resolutions (403).
 *          3. Reject symlinks (403) to prevent symlink attacks.
 *          4. Reject directory-like URIs (403) to avoid recursive deletion.
 *          5. Attempt file deletion with @ref unlinkFile, which validates
 *             type, permissions, and maps errors to proper responses.
 *          6. On success, build and return a 200 OK response containing
 *             a confirmation HTML page (via @ref generateDeleteHtml).
 *
 * @param req     The incoming HTTP request object.
 * @param server  Active server context.
 * @param loc     The matched location block from the configuration.
 *
 * @return A fully constructed @ref HttpResponse:
 *         - 200 OK with HTML confirmation if deletion succeeds.
 *         - Error response (403, 404, or 500) on failure.
 *
 * @ingroup request_handler
 */
HttpResponse handleDelete(const HttpRequest& req, const Server& server, const Location& loc) {
    // 1) Resolve the requested URI to a filesystem path
    std::string path = resolvePhysicalPath(req, loc);
    if (path.empty()) {
        Logger::logFrom(LogLevel::WARN, "Delete Handler",
                        "Path is empty → rejecting DELETE for URI: " + req.getPath());
        return ResponseBuilder::generateError(403, server, req);
    }

    // 2) Reject symbolic links to avoid symlink traversal exploits
    if (isSymlink(path)) {
        Logger::logFrom(LogLevel::WARN, "Delete Handler",
                        "Target is a symlink → rejecting DELETE for URI: " + req.getPath());
        return ResponseBuilder::generateError(403, server, req);
    }

    // 3) Reject URIs ending with '/' (treated as directories)
    if (req.getPath().back() == '/') {
        Logger::logFrom(LogLevel::WARN, "Delete Handler",
                        "Request URI ends with '/' → rejecting DELETE for directory-like path: " +
                            req.getPath());
        return ResponseBuilder::generateError(403, server, req);
    }

    // 4) Try to delete the file; errors are mapped to proper HTTP responses
    HttpResponse errResp;
    if (!unlinkFile(path, req, server, errResp)) {
        return errResp;
    }

    // 5) On success: build a sanitized confirmation page
    std::string rawName  = extractFilenameFromUri(req.getPath());
    std::string safeName = htmlEscape(rawName);          // escape to prevent XSS
    std::string body     = generateDeleteHtml(safeName); // build confirmation HTML
    Logger::logFrom(LogLevel::INFO, "Delete Handler",
                    "Successfully deleted “" + safeName + "” → sending HTML confirmation");

    // 6) Return a 200 OK response with confirmation body
    return ResponseBuilder::generateSuccess(200, body, "text/html", req);
}
