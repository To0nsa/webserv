/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   responseBuilder.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:14:23 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/19 09:31:28 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    responseBuilder.cpp
 * @brief   Implements the ResponseBuilder utilities for constructing HTTP responses.
 *
 * @details This file defines helper functions to generate HTTP responses of different
 *          types (success, file-based success, error, redirect). It encapsulates the
 *          boilerplate logic of setting status codes, headers, body content, and
 *          connection persistence rules based on HTTP version and `Connection` headers.
 *
 *          Key features:
 *          - Provides default status messages for standard HTTP codes
 *            via @ref MessageHandler::getDefaultMessage.
 *          - Centralizes connection management (keep-alive vs. close) in
 *            `initializeResponse`, consistent with HTTP/1.0 and HTTP/1.1 semantics.
 *          - Supports custom error pages configured in @ref Server instances,
 *            falling back to generated HTML when missing.
 *          - Generates file-backed responses (for static files or CGI output)
 *            with proper `Content-Length` and offset handling.
 *          - Generates redirect responses with appropriate `Location` headers.
 *
 *          These utilities are typically invoked by request handlers (GET, POST,
 *          DELETE, CGI, etc.) to produce fully-formed @ref HttpResponse objects
 *          that can be sent back to clients.
 *
 * @ingroup http
 */

#include "http/responseBuilder.hpp"
#include "core/Location.hpp"         // for Location
#include "core/Server.hpp"           // for Server
#include "http/HttpRequest.hpp"      // for HttpRequest
#include "http/HttpResponse.hpp"     // for HttpResponse
#include "utils/filesystemUtils.hpp" // for joinPath
#include "utils/htmlUtils.hpp"       // for htmlEscape
#include <fstream>                   // for basic_ostream, operator<<, basi...
#include <iterator>                  // for istreambuf_iterator, operator==
#include <map>                       // for operator==, map, _Rb_tree_const...
#include <set>                       // for set
#include <sstream>                   // for basic_ostringstream
#include <utility>                   // for pair
#include <vector>                    // for vector

/**
 * @namespace MessageHandler
 * @brief Utility namespace for HTTP status code reason phrases.
 *
 * @details Provides helpers to map numeric HTTP status codes
 *          (e.g., 200, 404, 500) to their standard reason phrases
 *          (e.g., "OK", "Not Found", "Internal Server Error").
 *
 *          Currently, this namespace exposes a single function:
 *          - @ref getDefaultMessage : Returns the standard reason phrase
 *            for a given status code, or `"Unknown Error"` if not found.
 *
 *          Used throughout response builders and handlers to ensure
 *          consistent status messages across all HTTP responses.
 *
 * @ingroup http
 */
namespace MessageHandler {
/**
 * @brief Returns the default reason phrase for a given HTTP status code.
 *
 * @details Uses a static lookup table of standard HTTP status codes
 *          (e.g., 200 → "OK", 404 → "Not Found"). The table is initialized
 *          once on first call and reused for all subsequent calls.
 *
 *          If the provided status code is not in the table, the function
 *          returns the fallback string `"Unknown Error"`.
 *
 * @param status_code The numeric HTTP status code (e.g., 200, 404, 500).
 * @return The corresponding reason phrase as a string, or `"Unknown Error"`
 *         if the code is not recognized.
 *
 * @ingroup http
 */
std::string getDefaultMessage(int status_code) {
    // Define a local alias for better readability and maintainability
    using StatusMessageMap = std::map<int, std::string>;
    // Static map ensures it's initialized once and reused on every call
    static const StatusMessageMap status_messages = {{200, "OK"},
                                                     {201, "Created"},
                                                     {301, "Moved Permanently"},
                                                     {302, "Found"},
                                                     {307, "Temporary Redirect"},
                                                     {308, "Permanent Redirect"},
                                                     {400, "Bad Request"},
                                                     {403, "Forbidden"},
                                                     {404, "Not Found"},
                                                     {405, "Method Not Allowed"},
                                                     {408, "Request Timeout"},
                                                     {411, "Length Required"},
                                                     {413, "Payload Too Large"},
                                                     {414, "Request-URI Too Long"},
                                                     {415, "Unsupported Media Type"},
                                                     {417, "Expectation Failed"},
                                                     {431, "Request Header Fields Too Large"},
                                                     {500, "Internal Server Error"},
                                                     {501, "Not Implemented"},
                                                     {502, "Bad Gateway"},
                                                     {503, "Service Unavailable"},
                                                     {504, "Gateway Timeout"},
                                                     {505, "HTTP Version Not Supported"}};

    // Attempt to find the status code in the map
    StatusMessageMap::const_iterator it = status_messages.find(status_code);
    // Return the associated message or a fallback if unknown
    return (it != status_messages.end()) ? it->second : "Unknown Error";
}
} // namespace MessageHandler

namespace {

/**
 * @brief Determines whether the TCP connection should be kept alive
 *        based on HTTP version and the `Connection` header.
 *
 * @details Implements protocol-specific keep-alive rules:
 *          - **HTTP/1.1**: persistent connections are the default.
 *            Only `"Connection: close"` disables keep-alive.
 *          - **HTTP/1.0**: connections close by default.
 *            Only `"Connection: keep-alive"` enables persistence.
 *          - **Other/unknown versions**: defaults to closing
 *            the connection for safety.
 *
 * @param version The HTTP version string from the request (e.g. `"HTTP/1.1"`).
 * @param conn    The value of the `Connection` header, lowercase-insensitive.
 * @return `true` if the connection may be kept alive, `false` if it should close.
 *
 * @ingroup http
 */
bool shouldKeepAlive(const std::string& version, const std::string& conn) {
    // HTTP/1.1 defaults to keep-alive unless explicitly closed
    if (version == "HTTP/1.1")
        return conn != "close";
    // HTTP/1.0 defaults to close unless explicitly kept alive
    if (version == "HTTP/1.0")
        return conn == "keep-alive";
    // Unknown or unsupported version → safest behavior: close
    return false;
}

/**
 * @brief Initializes a new HTTP response with status and connection metadata.
 *
 * @details This helper performs common setup for all responses:
 *          - Sets the numeric status code and reason phrase (e.g., `200 OK`).
 *          - Records the request's HTTP version and `Connection` header
 *            for later connection management.
 *          - Applies protocol-specific keep-alive rules via
 *            @ref shouldKeepAlive and closes the connection for certain
 *            error codes (400, 408, 413, 500).
 *          - Inserts the appropriate `Connection` header (`keep-alive` or `close`)
 *            into the response.
 *
 *          It centralizes connection-handling logic so that all
 *          response builders (`success`, `error`, `redirect`, etc.)
 *          stay consistent with HTTP/1.0 and HTTP/1.1 semantics.
 *
 * @param response     The @ref HttpResponse object being initialized.
 * @param status_code  The numeric HTTP status code (e.g., 200, 404, 500).
 * @param message      The reason phrase associated with the status code.
 * @param request      The original @ref HttpRequest, used to extract
 *                     version and `Connection` header metadata.
 *
 * @ingroup http
 */
void initializeResponse(HttpResponse& response, int status_code, const std::string& message,
                        const HttpRequest& request) {
    response.setStatus(status_code, message);
    // Store request version and connection header to guide connection persistence
    response.setRequestMeta(request.getVersion(), request.getHeader("Connection"));

    // Extract relevant headers from the request for keep-alive logic
    const std::string& conn    = request.getHeader("Connection");
    const std::string& version = request.getVersion();

    // These status codes indicate the server should always close the connection
    static const std::set<int> force_close_codes = {400, 408, 413, 500};

    // Determine if we should keep the connection alive based on protocol rules and status
    bool keep_alive = shouldKeepAlive(version, conn) && !force_close_codes.count(status_code);

    // Set the appropriate Connection header in the response
    response.setHeader("Connection", keep_alive ? "keep-alive" : "close");
}

} // anonymous namespace

/**
 * @namespace ResponseBuilder
 * @brief Factory helpers for constructing HTTP responses.
 *
 * @details The ResponseBuilder namespace groups together utility
 *          functions that generate fully-formed @ref HttpResponse
 *          objects for different scenarios:
 *
 *          - @ref generateSuccess : Build a success response with an inline body.
 *          - @ref generateSuccessFile : Build a response backed by a file (static file or CGI temp
 * file).
 *          - @ref generateError : Build an error response, using custom pages if available or a
 * default fallback.
 *          - @ref generateRedirect : Build a redirect response with a `Location` header.
 *
 *          All builders rely on @ref initializeResponse to apply
 *          consistent status metadata, HTTP version, and connection
 *          persistence rules. This ensures uniform behavior across
 *          all response types.
 *
 *          These functions are typically used by request handlers,
 *          CGI logic, and the router when constructing responses
 *          to be sent back to clients.
 *
 * @ingroup http
 */
namespace ResponseBuilder {
/**
 * @brief Builds a successful HTTP response with an inline body.
 *
 * @details This helper constructs a standard success response:
 *          - Looks up the default reason phrase for the given status code
 *            (e.g., `200 → "OK"`).
 *          - Initializes the response with status line, HTTP version, and
 *            connection persistence rules via @ref initializeResponse.
 *          - Sets the `Content-Type` header to the provided MIME type.
 *          - Attaches the supplied response body directly in memory.
 *
 *          Typical use cases include sending JSON, HTML, or plain-text
 *          responses for successful GET/POST requests.
 *
 * @param status_code   The HTTP status code to return (e.g., 200, 201).
 * @param body          The response body payload as a string.
 * @param content_type  The MIME type for the body (e.g., `"text/html"`,
 *                      `"application/json"`).
 * @param request       The original @ref HttpRequest, used to propagate
 *                      version and connection metadata.
 *
 * @return A fully constructed @ref HttpResponse object ready to send.
 *
 * @ingroup http
 */
HttpResponse generateSuccess(int status_code, const std::string& body,
                             const std::string& content_type, const HttpRequest& request) {
    HttpResponse response;
    std::string  message = MessageHandler::getDefaultMessage(status_code);

    // Set status, connection headers, and keep-alive logic
    initializeResponse(response, status_code, message, request);
    response.setHeader("Content-Type", content_type);
    response.setBody(body);
    return response;
}

/**
 * @brief Builds a successful HTTP response backed by a file on disk.
 *
 * @details This helper constructs a response intended for file or CGI output:
 *          - Looks up the default reason phrase for the given status code
 *            (e.g., `200 → "OK"`).
 *          - Initializes the response with status line, HTTP version, and
 *            connection persistence rules via @ref initializeResponse.
 *          - Sets `Content-Type` to the provided MIME type.
 *          - Sets `Content-Length` to the provided file size.
 *          - Records the CGI body offset (if applicable) to indicate where
 *            dynamic output begins inside a temporary file.
 *          - Stores the file path, allowing the networking layer to stream
 *            the file contents directly instead of keeping them in memory.
 *
 *          Typical use cases include serving static files (HTML, CSS, JS, images)
 *          or returning CGI-generated content that lives in a temporary file.
 *
 * @param status_code     The HTTP status code to return (e.g., 200, 206).
 * @param file_path       Filesystem path to the file that should be streamed.
 * @param content_type    The MIME type of the file (e.g., `"text/html"`,
 *                        `"application/octet-stream"`).
 * @param request         The original @ref HttpRequest, used to propagate
 *                        version and connection metadata.
 * @param content_length  Size of the file (or portion) to be served, in bytes.
 * @param cgiBodyOffset   Byte offset where the CGI response body begins,
 *                        if serving a CGI-generated temp file; usually 0 otherwise.
 *
 * @return A fully constructed @ref HttpResponse object configured for file streaming.
 *
 * @ingroup http
 */
HttpResponse generateSuccessFile(int status_code, const std::string& file_path,
                                 const std::string& content_type, const HttpRequest& request,
                                 std::streamsize content_length, std::streamsize cgiBodyOffset) {
    HttpResponse response;
    std::string  message = MessageHandler::getDefaultMessage(status_code);

    // Set status, connection headers, and keep-alive logic
    initializeResponse(response, status_code, message, request);
    response.setHeader("Content-Type", content_type);
    response.setHeader("Content-Length", std::to_string(content_length));
    response.setCgiBodyOffset(cgiBodyOffset);
    response.setFilePath(file_path);
    return response;
}

/**
 * @brief Builds an HTTP error response, optionally using a custom error page.
 *
 * @details This helper constructs error responses in two stages:
 *          1. It checks the server configuration for a custom error page
 *             matching the given status code (via @ref Server::getErrorPages).
 *             If found, it attempts to read the file and use its contents
 *             as the response body.
 *          2. If no custom page is configured or the file cannot be read,
 *             it falls back to generating a minimal default HTML page with
 *             the status code and reason phrase (e.g., `"404 Not Found"`).
 *
 *          In both cases, the response is initialized with correct status line,
 *          HTTP version, and connection persistence rules using
 *          @ref initializeResponse, and the `Content-Type` is set to `"text/html"`.
 *
 *          This ensures clients always receive a valid, informative error page,
 *          even if the custom configuration fails.
 *
 * @param status_code  The HTTP error code to return (e.g., 404, 500).
 * @param server       Reference to the @ref Server configuration,
 *                     used to look up custom error pages.
 * @param request      The original @ref HttpRequest, used for version and
 *                     connection metadata propagation.
 *
 * @return A fully constructed @ref HttpResponse object representing the error.
 *
 * @ingroup http
 */
HttpResponse generateError(int status_code, const Server& server, const HttpRequest& request) {
    HttpResponse response;
    std::string  message = MessageHandler::getDefaultMessage(status_code);
    std::string  body;

    // 1) Look for a custom error page in the server config
    const auto& error_pages = server.getErrorPages();
    auto        it          = error_pages.find(status_code);
    if (it != error_pages.end()) {
        std::string uri = it->second;

        // Strip leading slash to avoid double slashes in joinPath
        if (!uri.empty() && uri.front() == '/')
            uri.erase(0, 1);

        // Find the root path from the default "/" location
        std::string default_root;
        for (const auto& loc : server.getLocations()) {
            if (loc.getPath() == "/") {
                default_root = loc.getRoot();
                break;
            }
        }

        // Build full filesystem path and try to load the file
        std::string   full_path = joinPath(default_root, uri);
        std::ifstream file(full_path.c_str());
        if (file) {
            body.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        }
    }

    // 2) Fallback: generate a minimal default HTML error page
    if (body.empty()) {
        std::ostringstream ss;
        ss << "<html><body><h1>" << status_code << " " << htmlEscape(message)
           << "</h1></body></html>";
        body = ss.str();
    }

    // 3) Initialize the response metadata (status, headers, connection)
    initializeResponse(response, status_code, message, request);
    response.setHeader("Content-Type", "text/html");

    // 4) Attach the error body (custom or generated) ---
    if (!body.empty()) {
        response.setBody(body);
    }

    return response;
}

/**
 * @brief Generates an HTTP redirect response.
 *
 * @details Creates a response with the given redirect status code
 *          (e.g., 301 Moved Permanently, 302 Found, 307 Temporary Redirect),
 *          sets the appropriate `Location` header pointing to the target URL,
 *          and applies connection-handling logic via @ref initializeResponse.
 *
 *          By default, no response body is included for redirects, and most
 *          clients rely solely on the `Location` header. A `Content-Length: 0`
 *          header may be explicitly added if desired, but is optional.
 *
 * @param status_code Redirect status code (must be in the 3xx range).
 * @param location    Target URL or path for the redirect.
 * @param request     The original HTTP request (provides metadata for connection handling).
 *
 * @return An initialized @ref HttpResponse representing the redirect.
 *
 * @ingroup http
 */
HttpResponse generateRedirect(int status_code, const std::string& location,
                              const HttpRequest& request) {
    HttpResponse response;
    std::string  message = MessageHandler::getDefaultMessage(status_code);

    // Set status line, connection handling, and keep-alive logic
    initializeResponse(response, status_code, message, request);
    // Set the Location header to indicate the redirect target
    response.setHeader("Location", location);
    return response;
}

} // namespace ResponseBuilder
