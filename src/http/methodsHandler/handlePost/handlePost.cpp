/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handlePost.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/19 10:19:13 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/19 11:17:37 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    handlePost.cpp
 * @brief   Implements HTTP POST request handling logic.
 *
 * @details This file provides the full workflow for processing POST requests:
 *          - Validating request body size and upload configuration.
 *          - Normalizing and securing the target upload path against symlinks,
 *            escapes, and unsafe locations.
 *          - Supporting multiple content types:
 *              - `multipart/form-data` → handled by @ref handleMultipartForm.
 *              - `application/x-www-form-urlencoded` → parsed into key/value
 *                pairs and saved as HTML.
 *              - Raw body data → stored directly as a file.
 *          - Generating safe, unique filenames for uploads when needed.
 *          - Returning appropriate `HttpResponse` objects:
 *              - `201 Created` with confirmation on success.
 *              - Error responses (`400`, `403`, `404`, `413`, `500`) on invalid
 *                or failed operations.
 *
 * @ingroup request_handler
 */

#include "core/Location.hpp"         // for Location
#include "core/Server.hpp"           // for Server
#include "http/HttpRequest.hpp"      // for HttpRequest
#include "http/HttpResponse.hpp"     // for HttpResponse
#include "http/methodsHandler.hpp"   // for handleMultipartForm, handlePost
#include "http/responseBuilder.hpp"  // for generateError, generateSuccess
#include "utils/Logger.hpp"          // for LogLevel, Logger
#include "utils/filesystemUtils.hpp" // for normalizePath, isFile, joinPath
#include "utils/htmlUtils.hpp"       // for htmlEscape
#include "utils/urlUtils.hpp"        // for decodePercentEncoding, parseFor...
#include <atomic>                    // for atomic, memory_order_relaxed
#include <bits/chrono.h>             // for duration_cast, duration, nanose...
#include <filesystem>                // for path, exists, is_directory, is_...
#include <fstream>                   // for basic_ofstream, basic_ostream
#include <optional>                  // for optional, nullopt
#include <sstream>                   // for basic_ostringstream
#include <stdint.h>                  // for uint64_t
#include <string>                    // for operator+, allocator, char_traits
#include <system_error>              // for error_code
#include <unordered_map>             // for unordered_map, operator==, _Nod...
#include <utility>                   // for pair

namespace {

/**
 * @brief Handle a `application/x-www-form-urlencoded` POST request.
 *
 * @details
 * Parses the URL-encoded body of the request into key/value pairs,
 * generates a simple HTML confirmation page, and writes it to a file
 * at the given target path.
 *
 * Workflow:
 * - Parse form body using `parseFormUrlEncoded`.
 * - Reject with **400 Bad Request** if empty or malformed.
 * - Build an HTML document containing submitted form fields.
 * - Attempt to write the HTML to the specified file path.
 *   - On failure, return **500 Internal Server Error**.
 * - On success, return **201 Created** with confirmation HTML.
 *
 * @param request   The incoming HTTP request (provides form body).
 * @param server    The server context (used for error generation).
 * @param fullpath  Filesystem path where the HTML output should be saved.
 * @param filename  Suggested filename for confirmation message.
 * @return HttpResponse
 *   - 201 Created with confirmation HTML if saved successfully.
 *   - 400 Bad Request if form body is invalid.
 *   - 500 Internal Server Error if file operations fail.
 *
 * @ingroup request_handler
 */
static HttpResponse handleUrlEncodedForm(const HttpRequest& request, const Server& server,
                                         const std::string& fullpath, const std::string& filename) {
    // Parse form body into key/value pairs
    auto form = parseFormUrlEncoded(request.getBody());
    if (form.empty()) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "Empty or malformed URL-encoded form body from client.");
        return ResponseBuilder::generateError(400, server, request);
    }

    // Build an HTML page displaying submitted form fields
    std::string html = "<html><body><h1>Form Received</h1>";
    for (auto& formField : form) {
        html += "<p><b>" + htmlEscape(formField.first) + ":</b> " + htmlEscape(formField.second) +
                "</p>";
    }
    html += "</body></html>";

    // Try writing the HTML to the target file
    std::ofstream out(fullpath);
    if (!out.is_open()) {
        Logger::logFrom(LogLevel::ERROR, "Post Handler",
                        "Failed to open file for writing: " + fullpath);
        return ResponseBuilder::generateError(500, server, request);
    }

    out << html;
    out.close();
    if (out.fail()) {
        Logger::logFrom(LogLevel::ERROR, "Post Handler",
                        "Failed to write or close file: " + fullpath);
        return ResponseBuilder::generateError(500, server, request);
    }

    // Success: log and return confirmation response
    Logger::logFrom(LogLevel::INFO, "Post Handler",
                    "Successfully wrote URL-encoded form to: " + fullpath);
    return ResponseBuilder::generateSuccess(
        201, "<h1>Form Received. File " + filename + " created.</h1>", "text/html", request);
}

/**
 * @brief Handle a POST request with a raw body (no specific content type).
 *
 * @details
 * Writes the raw request body directly to the given filesystem path.
 * Primarily used for generic uploads when the content type is not
 * `multipart/form-data` or `application/x-www-form-urlencoded`.
 *
 * Workflow:
 * - Open target file for binary writing.
 *   - If opening fails, return **500 Internal Server Error**.
 * - Write the raw body from the request into the file.
 *   - If writing or closing fails, return **500 Internal Server Error**.
 * - On success, return **201 Created** with confirmation HTML.
 *
 * @param request   The incoming HTTP request (provides body to save).
 * @param server    The server context (used for error generation).
 * @param fullpath  Filesystem path where the body should be saved.
 * @param filename  Name of the saved file (for confirmation message).
 * @return HttpResponse
 *   - 201 Created with confirmation HTML if saved successfully.
 *   - 500 Internal Server Error if file I/O fails.
 *
 * @ingroup request_handler
 */
static HttpResponse handleRawBody(const HttpRequest& request, const Server& server,
                                  const std::string& fullpath, const std::string& filename) {
    // Try opening the target file in binary mode
    std::ofstream out(fullpath, std::ios::binary);
    if (!out.is_open()) {
        Logger::logFrom(LogLevel::ERROR, "Post Handler",
                        "Failed to open file for writing: " + fullpath);
        return ResponseBuilder::generateError(500, server, request);
    }

    // Write the raw request body into the file
    out << request.getBody();
    out.close();

    // Verify the write and close operations succeeded
    if (out.fail()) {
        Logger::logFrom(LogLevel::ERROR, "Post Handler",
                        "Failed to write or close file: " + fullpath);
        return ResponseBuilder::generateError(500, server, request);
    }

    // Success: log and return confirmation response
    Logger::logFrom(LogLevel::INFO, "Post Handler", "Successfully saved file to: " + fullpath);
    return ResponseBuilder::generateSuccess(
        201, "<html><body><h1>File " + htmlEscape(filename) + " created.</h1></body></html>",
        "text/html", request);
}

static std::atomic<uint64_t> uploadCounter{0};

/**
 * @brief Generate a unique filename for uploaded files.
 *
 * @details
 * This function ensures that every uploaded file gets a unique name
 * even under high concurrency.
 * It combines:
 * - **Current time in nanoseconds since epoch** (high resolution).
 * - **An atomic counter** (`uploadCounter`) to break ties when multiple
 *   uploads happen within the same nanosecond.
 *
 * The resulting filename follows the format:
 * ```
 * upload_<nanoseconds_since_epoch>_<sequence_number>
 * ```
 *
 * @return std::string
 *   A unique, collision-resistant filename suitable for uploads.
 *
 * @ingroup request_handler
 */
static std::string generateFilename() {
    // 1) Get high-resolution current time → nanoseconds since epoch
    auto     now = std::chrono::system_clock::now().time_since_epoch();
    uint64_t ns  = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

    // 2) Increment atomic counter to avoid collisions in the same nanosecond
    uint64_t seq = uploadCounter.fetch_add(1, std::memory_order_relaxed);

    // 3) Build the final filename using both components
    std::ostringstream oss;
    oss << "upload_" << ns << "_" << seq;
    return oss.str();
}

} // namespace

/**
 * @brief Validate the prerequisites for processing a POST request.
 *
 * @details
 * This function enforces three key checks before allowing a POST to proceed:
 * - Ensures that the request body is not empty (rejects with **400 Bad Request**).
 * - Ensures that the body size does not exceed the server's configured
 *   `client_max_body_size` (rejects with **413 Payload Too Large**).
 * - Ensures that the target location has a configured `upload_store`
 *   (rejects with **403 Forbidden**).
 *
 * If any of these conditions fail, an appropriate `HttpResponse` is generated.
 * Otherwise, the request is considered valid and processing can continue.
 *
 * @param request   The HTTP request to validate.
 * @param server    The server configuration, used to check limits.
 * @param location  The location configuration, used to check upload store availability.
 *
 * @return std::optional<HttpResponse>
 *   - `std::nullopt` if the request is valid.
 *   - A generated error response if validation fails.
 *
 * @ingroup request_handler
 */
static std::optional<HttpResponse>
validatePostRequest(HttpRequest const& request, Server const& server, Location const& location) {
    // 1) Reject empty bodies
    if (request.getBody().empty()) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "Empty body → rejecting POST for URI: " + request.getPath());
        return ResponseBuilder::generateError(400, server, request);
    }

    // 2) Reject requests exceeding configured max body size
    if (request.getBody().size() > server.getClientMaxBodySize()) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "Body size " + std::to_string(request.getBody().size()) +
                            " exceeds max client body size " +
                            std::to_string(server.getClientMaxBodySize()) +
                            " → rejecting POST for URI: " + request.getPath());
        return ResponseBuilder::generateError(413, server, request);
    }

    // 3) Reject if no upload store is configured for this location
    if (location.getUploadStore().empty()) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "No upload store configured → rejecting POST for URI: " +
                            request.getPath());
        return ResponseBuilder::generateError(403, server, request);
    }

    return std::nullopt;
}

namespace fs = std::filesystem;

/**
 * @brief Resolve and validate the filesystem path for a POST upload target.
 *
 * @details
 * This function computes the safe destination path for a file uploaded via POST,
 * ensuring it is confined within the configured `upload_store` of a location.
 * It enforces NGINX-like semantics and several security checks:
 * - Rejects POST requests to paths like `/file.ext/` if `file.ext` exists (→ 404).
 * - Decodes percent-encoded components (e.g. `%20`) in the request path.
 * - Builds the target path under the `upload_store`, appending a generated
 *   filename if the client POSTs to a directory or with a trailing slash.
 * - Canonicalizes the target path and checks that it remains within the upload root.
 * - Rejects paths containing symlinks to prevent traversal attacks (→ 403).
 * - Ensures the target directory exists, creating it if needed.
 * - Rejects uploads if the target file already exists (→ 400).
 *
 * On success, the function populates the output references with the resolved
 * path, directory, and filename to be used for writing the uploaded content.
 *
 * @param request            The incoming POST request.
 * @param server             The current server configuration (used for error responses).
 * @param location           The matched location configuration, with upload store info.
 * @param outTargetPath      Output: absolute safe path where the file will be written.
 * @param outTargetDirectory Output: directory part of the resolved path.
 * @param outTargetFilename  Output: filename part of the resolved path.
 *
 * @return std::optional<HttpResponse>
 *   - `std::nullopt` if the path is valid and safe for writing.
 *   - An error `HttpResponse` if validation fails.
 *
 * @ingroup request_handler
 */
static std::optional<HttpResponse>
preparePostTargetPath(HttpRequest const& request, Server const& server, Location const& location,
                      fs::path& outTargetPath, std::string& outTargetDirectory,
                      std::string& outTargetFilename) {
    namespace fs = std::filesystem;

    // 1) NGINX style: POST "/file.ext/" → 404 if file.ext exists
    std::string reqPath = normalizePath(request.getPath());
    std::string locPref = normalizePath(location.getPath());
    if (!reqPath.empty() && reqPath.back() == '/' && reqPath.rfind(locPref, 0) == 0) {
        std::string rel = reqPath.substr(locPref.size());
        while (!rel.empty() && rel.front() == '/')
            rel.erase(0, 1);
        std::string rootFull = joinPath(normalizePath(location.getRoot()), rel);
        if (!rootFull.empty() && rootFull.back() == '/')
            rootFull.pop_back();
        if (isFile(rootFull)) {
            Logger::logFrom(LogLevel::WARN, "Post Handler",
                            "Trailing slash on file → rejecting POST for URI: " +
                                request.getPath());
            return ResponseBuilder::generateError(404, server, request);
        }
    }

    // 2) Compute the client‐side relative path under the upload_store
    std::string rawRel = reqPath.substr(locPref.size());
    while (!rawRel.empty() && rawRel.front() == '/')
        rawRel.erase(0, 1);

    // 3) Percent‐decode any “%20”, etc.
    std::string decodedRel = decodePercentEncoding(rawRel);

    // 4) Build the candidate path under the uploadStore
    fs::path uploadRoot(location.getUploadStore());
    fs::path candidate = uploadRoot / fs::path(decodedRel);

    // 5) If they POST to a directory (or included a trailing slash), append a generated filename
    bool endsWithSlash = !reqPath.empty() && reqPath.back() == '/';
    if (endsWithSlash || (fs::exists(candidate) && fs::is_directory(candidate))) {
        candidate /= generateFilename();
    }

    // 6) Canonicalize & bound‐check (reuse your makeSafeUploadPath taking a relative path)
    //    First compute the path _relative_ to uploadRoot:
    fs::path    relCand    = candidate.lexically_relative(uploadRoot);
    std::string relCandStr = relCand.generic_string();

    std::string safeFull = makeSafeUploadPath(location.getUploadStore(), relCandStr);
    if (safeFull.empty()) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "Unsafe upload path → rejecting POST for URI: " + request.getPath() +
                            " rel: " + relCandStr);
        return ResponseBuilder::generateError(403, server, request);
    }

    // 7) Symlink-forbid: reject if any component under uploadRoot is a symlink
    fs::path root = uploadRoot;
    fs::path target(outTargetPath = safeFull);
    fs::path relRooted = target.lexically_relative(root);
    fs::path acc       = root;
    for (auto const& comp : relRooted) {
        acc /= comp;
        std::error_code ec;
        if (fs::is_symlink(acc, ec) && !ec) {
            Logger::logFrom(LogLevel::WARN, "Post Handler",
                            "Symlink in upload path → rejecting POST for URI: " +
                                request.getPath() + " component: " + acc.string());
            return ResponseBuilder::generateError(403, server, request);
        }
    }

    // 8) Populate outputs and ensure directory exists
    outTargetPath      = fs::path(safeFull);
    outTargetDirectory = outTargetPath.parent_path().generic_string();
    outTargetFilename  = outTargetPath.filename().string();

    if (!mkdirRecursive(outTargetDirectory)) {
        Logger::logFrom(LogLevel::ERROR, "Post Handler",
                        "Failed to create directory: " + outTargetDirectory +
                            " → rejecting POST for URI: " + request.getPath());
        return ResponseBuilder::generateError(500, server, request);
    }

    // 9) Reject if file already exists
    if (isFile(outTargetPath.string())) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "File already exists → rejecting POST for URI: " + request.getPath() +
                            " path: " + outTargetPath.string());
        return ResponseBuilder::generateError(400, server, request);
    }

    // 10) All clear
    return std::nullopt;
}

/**
 * @brief Dispatch POST handling based on the request's Content-Type.
 *
 * @details Chooses the appropriate handler:
 *          - **multipart/form-data** → @ref handleMultipartForm (saves parts into @p
 * targetDirectory).
 *          - **application/x-www-form-urlencoded** → @ref handleUrlEncodedForm (writes parsed HTML
 * to @p targetPath).
 *          - **(fallback)** any other/absent type → @ref handleRawBody (dumps raw body to @p
 * targetPath).
 *
 *          This function relies on a simple substring match of the `Content-Type`
 *          header. If you later support parameters (e.g. `charset=...`) or more
 *          types, consider normalizing and parsing the media type token.
 *
 * @param request         Incoming HTTP request (provides headers/body).
 * @param server          Server context (for error responses).
 * @param targetPath      Absolute file path chosen for single-file saves.
 * @param targetDirectory Directory path to store multipart parts.
 * @param targetFilename  Leaf filename for single-file saves.
 *
 * @return A populated @ref HttpResponse produced by the selected handler.
 *
 * @ingroup request_handler
 */
static HttpResponse dispatchPostByContentType(HttpRequest const& request, const Server& server,
                                              std::filesystem::path const& targetPath,
                                              std::string const&           targetDirectory,
                                              std::string const&           targetFilename) {
    // Read Content-Type once; could be empty or contain parameters (e.g., boundary/charset)
    std::string contentTypeHeader = request.getHeader("Content-Type");

    // Multipart form → delegate to multipart handler (uses targetDirectory)
    if (contentTypeHeader.find("multipart/form-data") != std::string::npos) {
        return handleMultipartForm(request, server, targetDirectory);
    }

    // URL-encoded form → parse and render confirmation HTML to targetPath
    if (contentTypeHeader.find("application/x-www-form-urlencoded") != std::string::npos) {
        return handleUrlEncodedForm(request, server, targetPath.string(), targetFilename);
    }

    // Fallback → store raw body verbatim at targetPath
    return handleRawBody(request, server, targetPath.string(), targetFilename);
}

/**
 * @brief Handles an HTTP POST request end-to-end.
 *
 * @details Pipeline:
 *   1) **Preconditions** — validate body presence/size and that the @ref Location
 *      has an `upload_store` (see @ref validatePostRequest). Returns 400/413/403 on failure.
 *   2) **Target path preparation** — resolve a safe destination under the
 *      location's upload store, forbid traversal/symlinks, create parent dirs,
 *      and choose a filename if needed (see @ref preparePostTargetPath). Returns
 *      404/403/500/400 on failure per checks.
 *   3) **Dispatch by Content-Type** — select the concrete handler:
 *      - `multipart/form-data` → @ref handleMultipartForm
 *      - `application/x-www-form-urlencoded` → @ref handleUrlEncodedForm
 *      - otherwise → @ref handleRawBody
 *
 * On success, returns a `201 Created`; on errors, returns the appropriate
 * generated @ref HttpResponse from earlier stages.
 *
 * @param request   Incoming HTTP request.
 * @param server    Active server context.
 * @param location  Matched location configuration (upload policy, paths).
 * @return A fully constructed @ref HttpResponse.
 *
 * @ingroup request_handler
 */
HttpResponse handlePost(HttpRequest const& request, Server const& server,
                        Location const& location) {
    // 1) Preconditions: body present/within limits, upload_store configured
    std::optional<HttpResponse> maybeErrorResponse = validatePostRequest(request, server, location);
    if (maybeErrorResponse.has_value()) {
        return *maybeErrorResponse;
    }

    // 2) Prepare a safe target path under upload_store (dirs, filename, symlink checks)
    std::filesystem::path targetPath;
    std::string           targetDirectory;
    std::string           targetFilename;

    std::optional<HttpResponse> maybePreparationError = preparePostTargetPath(
        request, server, location, targetPath, targetDirectory, targetFilename);
    if (maybePreparationError.has_value()) {
        return *maybePreparationError;
    }

    // 3) Route to the proper POST handler based on Content-Type
    return dispatchPostByContentType(request, server, targetPath, targetDirectory, targetFilename);
}
