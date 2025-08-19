/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 10:56:54 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/19 09:17:55 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    HttpResponse.cpp
 * @brief   Implements the HttpResponse class.
 *
 * @details Defines all methods of @ref HttpResponse, including setters for
 *          status, headers, body, file path, and CGI metadata; getters and
 *          queries for response properties; and utilities such as
 *          `toHttpString()` for serializing the response into HTTP wire format.
 *
 *          The implementation also includes connection management logic
 *          (`isConnectionClose`) that follows HTTP/1.0 and HTTP/1.1 semantics,
 *          as well as RFC-friendly handling for certain error status codes.
 *
 *          Instances of this class are typically produced by
 *          @ref ResponseBuilder and HTTP method handlers, then written back
 *          to the client by the networking layer.
 *
 * @ingroup http
 */

#include "http/HttpResponse.hpp"
#include <algorithm> // for transform
#include <ctype.h>   // for tolower
#include <set>       // for set
#include <sstream>   // for basic_ostream, operator<<, basic_stringstream
#include <utility>   // for pair

//=== Construction & Special Members =====================================

/**
 * @brief Constructs a default HttpResponse with status `200 OK`.
 *
 * @details Initializes response with status code 200, message `"OK"`,
 *          and a CGI body offset of `0`. Other fields are left empty
 *          until explicitly set.
 *
 * @ingroup http
 */
HttpResponse::HttpResponse(void) {
    _status_code    = 200;
    _status_message = "OK";
    _cgiBodyOffset  = 0;
}

/**
 * @brief Destroys the HttpResponse.
 *
 * @details Provided for completeness; does not manage external resources.
 *
 * @ingroup http
 */
HttpResponse::~HttpResponse(void) {
}

//=== Mutators (Setters) ==================================================

/**
 * @brief Sets the status code and reason phrase of the response.
 * @param code Numeric status code (e.g., 404).
 * @param message Reason phrase (e.g., "Not Found").
 * @ingroup http
 */
void HttpResponse::setStatus(int code, const std::string& message) {
    _status_code    = code;
    _status_message = message;
}

/**
 * @brief Adds or replaces a header in the response.
 * @param key Header name.
 * @param value Header value.
 * @ingroup http
 */
void HttpResponse::setHeader(const std::string& key, const std::string& value) {
    _headers[key] = value;
}

/**
 * @brief Sets the body of the response.
 * @param body Response payload as a string.
 * @ingroup http
 */
void HttpResponse::setBody(const std::string& body) {
    _body = body;
}

/**
 * @brief Stores HTTP version and connection header metadata.
 *
 * @param version HTTP version string (e.g., "HTTP/1.1").
 * @param conn Raw value of the "Connection" header from the request.
 * @ingroup http
 */
void HttpResponse::setRequestMeta(const std::string& version, const std::string& conn) {
    _http_version      = version;
    _connection_header = conn;
}

/**
 * @brief Sets a file path for file-backed responses.
 * @param path Filesystem path to serve.
 * @ingroup http
 */
void HttpResponse::setFilePath(const std::string& path) {
    _file_path = path;
}

/**
 * @brief Sets the byte offset where CGI body begins in its temp file.
 * @param offset Offset in bytes.
 * @ingroup http
 */
void HttpResponse::setCgiBodyOffset(std::streamsize offset) {
    _cgiBodyOffset = offset;
}

/**
 * @brief Sets the temporary file path produced by CGI.
 * @param temp_file Path to the CGI temp file.
 * @ingroup http
 */
void HttpResponse::setCgiTempFile(const std::string& temp_file) {
    _cgi_temp_file = temp_file;
}

//=== Queries (Getters) ===================================================

/**
 * @brief Serializes the response into HTTP wire format.
 *
 * @details Produces a string containing the status line, headers,
 *          `Content-Length`, and body, ready for transmission.
 *
 * @return HTTP response string.
 * @ingroup http
 */
std::string HttpResponse::toHttpString(void) const {
    std::stringstream ss;

    ss << "HTTP/1.1 " << _status_code << " " << _status_message << "\r\n";

    for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
         it != _headers.end(); ++it)
        ss << it->first << ": " << it->second << "\r\n";

    ss << "Content-Length: " << _body.length() << "\r\n";
    ss << "\r\n";
    ss << _body;

    return ss.str();
}

/**
 * @brief Returns the file path of the response, if any.
 * @ingroup http
 */
const std::string& HttpResponse::getFilePath() const {
    return _file_path;
}

/**
 * @brief Returns the numeric status code.
 * @ingroup http
 */
int HttpResponse::getStatusCode(void) const {
    return _status_code;
}

/**
 * @brief Returns the reason phrase of the response.
 * @ingroup http
 */
const std::string& HttpResponse::getStatusMessage(void) const {
    return _status_message;
}

/**
 * @brief Returns the headers of the response.
 * @ingroup http
 */
const std::map<std::string, std::string>& HttpResponse::getHeaders(void) const {
    return _headers;
}

/**
 * @brief Returns the CGI body offset.
 * @ingroup http
 */
std::streamsize HttpResponse::getCgiBodyOffset() const {
    return _cgiBodyOffset;
}

/**
 * @brief Returns the CGI temporary file path.
 * @ingroup http
 */
const std::string& HttpResponse::getCgiTempFile() const {
    return _cgi_temp_file;
}

//=== Predicates & Utilities ==============================================

/**
 * @brief Determines whether the server should close the connection
 *        after sending this response.
 *
 * @details The decision follows HTTP semantics and some defensive RFC-inspired
 *          rules:
 *          1. **Explicit Connection header in the response**:
 *             If present, its value takes precedence (case-insensitive).
 *             - `"Connection: close"` → close the connection.
 *             - Any other value → keep open, unless other rules apply.
 *
 *          2. **Force-close status codes**:
 *             Certain error codes (400, 408, 413, 500) always require closing
 *             the connection to remain protocol-compliant and prevent reuse
 *             of a potentially invalid connection.
 *
 *          3. **Fallback to request metadata**:
 *             - For HTTP/1.1: keep-alive by default, unless request said `"close"`.
 *             - For HTTP/1.0: close by default, unless request said `"keep-alive"`.
 *
 *          4. **Unknown HTTP version**:
 *             Defaults to closing the connection for safety.
 *
 * @return `true` if the server must close the TCP connection,
 *         `false` if it can be kept alive.
 *
 * @ingroup http
 */
bool HttpResponse::isConnectionClose() const {
    // 1) If the response explicitly sets Connection, honor it.
    std::map<std::string, std::string>::const_iterator hit = _headers.find("Connection");
    if (hit != _headers.end()) {
        std::string v = hit->second;
        std::transform(v.begin(), v.end(), v.begin(), ::tolower);
        return v == "close";
    }

    // 2) Certain status codes must close (RFC-friendly behavior).
    static const std::set<int> force_close_codes = {400, 408, 413, 500};
    if (force_close_codes.count(_status_code))
        return true;

    // 3) Fall back to protocol semantics using the request metadata.
    std::string conn = _connection_header;
    std::transform(conn.begin(), conn.end(), conn.begin(), ::tolower);

    if (_http_version == "HTTP/1.1") {
        // Keep-alive by default unless client asked to close
        return conn == "close";
    }
    if (_http_version == "HTTP/1.0") {
        // Close by default unless client asked to keep-alive
        return conn != "keep-alive";
    }

    // Unknown version: safest is to close.
    return true;
}

/**
 * @brief Returns true if the response serves a file from disk.
 * @ingroup http
 */
bool HttpResponse::isFileResponse() const {
    return !_file_path.empty();
}

/**
 * @brief Returns true if a CGI temp file is set.
 * @ingroup http
 */
bool HttpResponse::isCgiTempFile() const {
    return !_cgi_temp_file.empty();
}
