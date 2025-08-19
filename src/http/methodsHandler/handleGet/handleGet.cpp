/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleGet.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/15 12:39:41 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/19 10:46:55 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    handleGet.cpp
 * @brief   Implements the HTTP GET request handler.
 *
 * @details This file provides the logic for serving resources in response
 *          to HTTP GET requests. It includes:
 *          - **MIME type detection** (@ref detectMimeType) for correct
 *            `Content-Type` headers.
 *          - **File serving** (@ref serveFile) with small-file in-memory
 *            responses and large-file streaming responses.
 *          - **Directory handling** (@ref processDirectory):
 *              - Redirects URIs missing a trailing slash.
 *              - Serves configured `index` files if present.
 *              - Generates autoindex listings if enabled.
 *              - Returns 403 when directory listing is disabled.
 *          - **Main dispatcher** (@ref handleGet) which ties everything
 *            together, enforcing nginx-like behavior:
 *              - Rejects symlinks for security.
 *              - Differentiates between directories and regular files.
 *              - Returns appropriate errors (403/404) when access is denied
 *                or resources are missing.
 *
 * @ingroup request_handler
 */

#include "core/Location.hpp"         // for Location
#include "http/HttpRequest.hpp"      // for HttpRequest
#include "http/HttpResponse.hpp"     // for HttpResponse
#include "http/methodsHandler.hpp"   // for generateAutoindex, handleGet
#include "http/responseBuilder.hpp"  // for generateError, generateRedirect
#include "utils/Logger.hpp"          // for LogLevel, Logger
#include "utils/filesystemUtils.hpp" // for isFile, isSymlink, joinPath
#include <algorithm>                 // for transform
#include <cctype>                    // for tolower
#include <filesystem>                // for path
#include <fstream>                   // for basic_ifstream, basic_ios, ios
#include <map>                       // for map, operator==, _Rb_tree_const...
#include <sstream>                   // for basic_ostringstream
#include <string>                    // for allocator, operator+, char_traits
#include <sys/stat.h>                // for stat, S_ISDIR, S_ISREG
#include <utility>                   // for pair
class Server;

namespace {

/**
 * @brief Detects the MIME type of a file based on its extension.
 *
 * @details Uses a static lookup table mapping common file extensions
 *          (e.g., `.html`, `.png`, `.json`) to their corresponding
 *          MIME types. The file extension is normalized to lowercase
 *          before lookup. If the extension is not recognized, the
 *          generic `application/octet-stream` type is returned.
 *
 * @param file_path Path to the file whose MIME type should be determined.
 *
 * @return A MIME type string suitable for the `Content-Type` header.
 *
 * @ingroup request_handler
 */
static std::string detectMimeType(const std::string& file_path) {
    // Static lookup table of common extensions → MIME types
    static const std::map<std::string, std::string> mime_types = {
        {".html", "text/html"},        {".htm", "text/html"},
        {".css", "text/css"},          {".js", "application/javascript"},
        {".json", "application/json"}, {".txt", "text/plain"},
        {".jpg", "image/jpeg"},        {".jpeg", "image/jpeg"},
        {".png", "image/png"},         {".gif", "image/gif"},
        {".svg", "image/svg+xml"},     {".ico", "image/x-icon"},
        {".pdf", "application/pdf"},   {".zip", "application/zip"},
        {".tar", "application/x-tar"}, {".xml", "application/xml"},
        {".mp3", "audio/mpeg"},        {".mp4", "video/mp4"},
        {".wasm", "application/wasm"}};

    // Extract file extension from path
    std::filesystem::path fsPath(file_path);
    std::string           ext = fsPath.extension().string();

    // Normalize extension to lowercase for lookup
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Return known type if found, else default to binary
    auto it = mime_types.find(ext);
    if (it != mime_types.end())
        return it->second;

    return "application/octet-stream";
}

/**
 * @brief Serves a static file in response to an HTTP GET request.
 *
 * @details This helper reads a file from disk and builds the
 *          appropriate @ref HttpResponse:
 *          - Verifies that the target is a valid file.
 *          - Opens the file in binary mode, rejects if inaccessible.
 *          - Determines the `Content-Type` header (uses
 *            @ref detectMimeType if none is provided).
 *          - If the file size is small (≤ 100 KiB), reads the
 *            entire file into memory and returns it inline.
 *          - If the file size is larger, streams it back using
 *            @ref ResponseBuilder::generateSuccessFile.
 *
 *          Logs each decision (missing file, forbidden, in-memory,
 *          or streaming).
 *
 * @param file_path    Filesystem path to the target file.
 * @param request      Incoming HTTP request.
 * @param server       Active server context (for error generation).
 * @param content_type Optional MIME type override; if empty, auto-detected.
 *
 * @return A valid @ref HttpResponse:
 *         - 200 with body or stream on success.
 *         - 404 if file not found.
 *         - 403 if access is denied.
 *
 * @ingroup request_handler
 */
static HttpResponse serveFile(const std::string& file_path, const HttpRequest& request,
                              const Server& server, std::string content_type) {
    // 1) Check if the path is a file
    if (!isFile(file_path)) {
        Logger::logFrom(LogLevel::WARN, "Get Handler", "File not found: " + file_path);
        return ResponseBuilder::generateError(404, server, request);
    }

    // 2) Attempt to open the file
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        Logger::logFrom(LogLevel::WARN, "Get Handler", "Cannot open file: " + file_path);
        return ResponseBuilder::generateError(403, server, request);
    }

    // 3) Determine file size
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // 4) Detect MIME type if not provided
    if (content_type.empty())
        content_type = detectMimeType(file_path);

    // 5) Small files (≤100 KiB) → load fully into memory
    constexpr std::streamsize MEMORY_LIMIT = 100 * 1024;
    if (size <= MEMORY_LIMIT) {
        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string body = buffer.str();

        Logger::logFrom(LogLevel::INFO, "Get Handler",
                        "Serving small file in-memory (size: " + std::to_string(size) +
                            " bytes): " + file_path);
        return ResponseBuilder::generateSuccess(200, body, content_type, request);
    } else {
        // 6) Large files → return as streaming response
        Logger::logFrom(LogLevel::INFO, "Get Handler",
                        "Serving large file via streaming (size: " + std::to_string(size) +
                            " bytes): " + file_path);
        return ResponseBuilder::generateSuccessFile(200, file_path, content_type, request, size);
    }
}

/**
 * @brief Handles HTTP GET requests targeting a directory.
 *
 * @details This function enforces nginx-like directory handling rules:
 *          1. Normalize the URI and reject empty results (403).
 *          2. If the URI does not end with a slash, issue a `301 Moved Permanently`
 *             redirect to the slash-terminated URI.
 *          3. If an `index` file is configured:
 *              - Serve it if it exists.
 *              - Otherwise, if autoindex is enabled, generate a directory listing.
 *              - Otherwise, return 403 (listing disabled).
 *          4. If no index is configured:
 *              - Generate autoindex if enabled.
 *              - Otherwise, return 403 (directory listing forbidden).
 *
 * @param dirPath  Physical filesystem path to the requested directory.
 * @param uri      Original request URI.
 * @param request  Incoming HTTP request.
 * @param server   Active server context.
 * @param loc      The matched location block for this URI.
 *
 * @return A fully built @ref HttpResponse:
 *         - 301 Redirect if missing slash.
 *         - 200 OK with index file or autoindex if allowed.
 *         - 403 Forbidden if neither index nor autoindex is permitted.
 *
 * @ingroup request_handler
 */
static HttpResponse processDirectory(const std::string& dirPath, const std::string& uri,
                                     const HttpRequest& request, const Server& server,
                                     const Location& loc) {
    // 1) Normalize URI and reject empty (security check)
    std::string normalized = normalizePath(uri);
    if (normalized.empty()) {
        Logger::logFrom(LogLevel::WARN, "Get Handler",
                        "Normalized URI is empty (possible escape) for URI: " + uri);
        return ResponseBuilder::generateError(403, server, request);
    }

    // 2) Redirect if URI does not end with '/'
    if (normalized.back() != '/') {
        std::string target = normalized + "/";
        Logger::logFrom(LogLevel::INFO, "Get Handler", "Redirecting to: " + target);
        return ResponseBuilder::generateRedirect(301, target, request);
    }

    // 3) If an index file is configured in the location
    const std::string& indexName = loc.getIndex();
    if (!indexName.empty()) {
        std::string indexFullPath = joinPath(dirPath, indexName);
        if (isFile(indexFullPath)) {
            return serveFile(indexFullPath, request, server,
                             ""); // serve index.html (or configured name)
        }

        if (loc.isAutoindexEnabled()) {
            return generateAutoindex(dirPath, uri, request, server); // fallback to autoindex
        }

        Logger::logFrom(LogLevel::WARN, "Get Handler",
                        "Index file not found and autoindex disabled for: " + dirPath);
        return ResponseBuilder::generateError(403, server, request);
    }

    // 4) If no index: allow autoindex or forbid
    if (loc.isAutoindexEnabled()) {
        return generateAutoindex(dirPath, uri, request, server);
    }

    Logger::logFrom(LogLevel::WARN, "Get Handler",
                    "Directory listing not allowed (autoindex disabled) for: " + dirPath);
    return ResponseBuilder::generateError(403, server, request);
}

} // namespace

/**
 * @brief Handles an HTTP GET request for a resource.
 *
 * @details This function implements nginx-like GET semantics:
 *          1. Resolve the request URI to a physical filesystem path.
 *          2. If the URI ends with a `/` but points to a file (e.g., `/file/`):
 *             - Reject with 404 (trailing slash on file).
 *          3. Reject symlinks for security (403).
 *          4. If the path exists:
 *              - If it’s a directory, delegate to @ref processDirectory
 *                (redirect, index, or autoindex handling).
 *              - If it’s a regular file, delegate to @ref serveFile
 *                (in-memory or streaming response).
 *          5. If nothing matches, return 404 (not found).
 *
 * @param request Incoming HTTP request object.
 * @param server  Active server context.
 * @param loc     The matched location block for this URI.
 *
 * @return A fully constructed @ref HttpResponse:
 *         - 200 OK with file contents or directory listing.
 *         - 301 Redirect for missing slash.
 *         - 403 Forbidden for symlinks or disallowed directories.
 *         - 404 Not Found when resource doesn’t exist.
 *
 * @ingroup request_handler
 */
HttpResponse handleGet(const HttpRequest& request, const Server& server, const Location& loc) {
    // 1) Resolve request path to a filesystem path
    std::string filepath = resolvePhysicalPath(request, loc);

    // 2) Special nginx rule: if URI ends with '/' but points to a file → 404
    const std::string& uri = request.getPath();
    if (!uri.empty() && uri.back() == '/') {
        std::string fileNoSlash = filepath;
        if (!fileNoSlash.empty() && fileNoSlash.back() == '/')
            fileNoSlash.pop_back();
        if (isFile(fileNoSlash)) {
            Logger::logFrom(LogLevel::WARN, "Get Handler",
                            "Trailing slash on file → returning 404 for URI: " + uri);
            return ResponseBuilder::generateError(404, server, request);
        }
    }

    // 3) Reject symlinks for security
    if (isSymlink(filepath)) {
        Logger::logFrom(LogLevel::WARN, "Get Handler",
                        "Symlink detected, rejecting request for: " + filepath);
        return ResponseBuilder::generateError(403, server, request);
    }

    // 4) Check file type
    struct stat fileStat;
    if (stat(filepath.c_str(), &fileStat) == 0) {
        const std::string& uri = request.getPath();

        if (S_ISDIR(fileStat.st_mode)) {
            // Directory → delegate to processDirectory
            return processDirectory(filepath, uri, request, server, loc);
        }

        if (S_ISREG(fileStat.st_mode)) {
            // Regular file → delegate to serveFile
            return serveFile(filepath, request, server, "");
        }
    }

    // 5) Not found or inaccessible
    Logger::logFrom(LogLevel::WARN, "Get Handler", "File not found or inaccessible: " + filepath);
    return ResponseBuilder::generateError(404, server, request);
}
