/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:31:58 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/19 09:07:56 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    HttpRequest.cpp
 * @brief   Implements the HttpRequest class.
 *
 * @details Defines all methods of @ref HttpRequest for managing the request line,
 *          headers, body, query string, and metadata extracted during parsing.
 *          Includes debugging utilities such as `printRequest()` and normalization
 *          of header keys. This class is the main container populated by
 *          @ref HttpRequestParser and consumed by the router and HTTP method
 *          handlers.
 *
 * @ingroup http
 */

#include "http/HttpRequest.hpp"
#include "utils/stringUtils.hpp" // for toUpper
#include <algorithm>             // for all_of, min
#include <cctype>                // for isprint
#include <iomanip>               // for operator<<, setw
#include <iostream>              // for basic_ostream, operator<<, cout, left
#include <utility>               // for pair

//=== Construction & Special Members =====================================

/**
 * @brief Constructs an empty HttpRequest.
 *
 * @details Initializes fields to default values, including setting
 *          `_matchedServerIndex` to `0` (first server).
 *
 * @ingroup http
 */
HttpRequest::HttpRequest(void) {
    _matchedServerIndex = 0; // Default to the first server
}

/**
 * @brief Destroys the HttpRequest.
 *
 * @details Provided for completeness; does not manage dynamic resources.
 *
 * @ingroup http
 */
HttpRequest::~HttpRequest(void) {
}

//=== Debug & Utilities ===================================================

/**
 * @brief Prints the request in human-readable form.
 *
 * @details Dumps method, path, query string, version, headers, and body.
 *          Body output is truncated to 100 characters and marks binary data.
 *
 * @ingroup http
 */
void HttpRequest::printRequest() const {
    // ... (implementation unchanged)
}

//=== Queries (Getters) ===================================================

/**
 * @brief Returns the HTTP method (e.g. "GET", "POST").
 * @ingroup http
 */
const std::string& HttpRequest::getMethod(void) const {
    return _method;
}

/**
 * @brief Returns the normalized request path.
 * @ingroup http
 */
const std::string& HttpRequest::getPath(void) const {
    return _path;
}

/**
 * @brief Returns the HTTP version string (e.g. "HTTP/1.1").
 * @ingroup http
 */
const std::string& HttpRequest::getVersion(void) const {
    return _version;
}

/**
 * @brief Returns the value of a header.
 *
 * @details Header lookup is case-insensitive. If the header is not present,
 *          returns a reference to a static empty string.
 *
 * @param key Header name to look up.
 * @return Reference to the header value, or an empty string if not found.
 * @ingroup http
 */
const std::string& HttpRequest::getHeader(const std::string& key) const {
    const std::string                                  upperKey = toUpper(key);
    static const std::string                           empty    = "";
    std::map<std::string, std::string>::const_iterator it       = _headers.find(upperKey);
    if (it != _headers.end())
        return it->second;
    return empty;
}

/**
 * @brief Returns the full header map.
 * @ingroup http
 */
const std::map<std::string, std::string>& HttpRequest::getHeaders() const {
    return _headers;
}

/**
 * @brief Returns the request body string.
 * @ingroup http
 */
const std::string& HttpRequest::getBody(void) const {
    return _body;
}

/**
 * @brief Returns the declared Content-Length.
 * @ingroup http
 */
std::size_t HttpRequest::getContentLength(void) const {
    return _contentLength;
}

/**
 * @brief Returns the query string after "?" in URI.
 * @ingroup http
 */
const std::string& HttpRequest::getQuery() const {
    return _query;
}

/**
 * @brief Returns the parse error code.
 * @ingroup http
 */
int HttpRequest::getParseErrorCode(void) const {
    return _parseError;
}

/**
 * @brief Returns the index of the matched server block.
 * @ingroup http
 */
int HttpRequest::getMatchedServerIndex() const {
    return _matchedServerIndex;
}

/**
 * @brief Returns the normalized Host header (lowercased).
 * @ingroup http
 */
const std::string& HttpRequest::getHost() const {
    return _host;
}

//=== Mutators (Setters) ==================================================

/**
 * @brief Sets the HTTP method string.
 * @param method Method name (e.g., "GET").
 * @ingroup http
 */
void HttpRequest::setMethod(const std::string& method) {
    _method = method;
}

/**
 * @brief Sets the normalized request path.
 * @param path Request target path.
 * @ingroup http
 */
void HttpRequest::setPath(const std::string& path) {
    _path = path;
}

/**
 * @brief Sets the HTTP version string.
 * @param version Version string (e.g., "HTTP/1.1").
 * @ingroup http
 */
void HttpRequest::setVersion(const std::string& version) {
    _version = version;
}

/**
 * @brief Adds or replaces a header value.
 *
 * @details Keys are stored in uppercase to allow case-insensitive lookups.
 *
 * @param key   Header name.
 * @param value Header value.
 * @ingroup http
 */
void HttpRequest::setHeader(const std::string& key, const std::string& value) {
    const std::string upperKey = toUpper(key);
    _headers[upperKey]         = value;
}

/**
 * @brief Sets the request body string.
 * @param body Raw body payload.
 * @ingroup http
 */
void HttpRequest::setBody(const std::string& body) {
    _body = body;
}

/**
 * @brief Sets the Content-Length value.
 * @param len Length in bytes.
 * @ingroup http
 */
void HttpRequest::setContentLength(size_t len) {
    _contentLength = len;
}

/**
 * @brief Sets the parsed URL object.
 * @param url Parsed URL reference.
 * @ingroup http
 */
void HttpRequest::setUrl(const Url& url) {
    _url = url;
}

/**
 * @brief Sets the query string.
 * @param query Raw query string.
 * @ingroup http
 */
void HttpRequest::setQuery(const std::string& query) {
    _query = query;
}

/**
 * @brief Sets the parse error code.
 * @param error Error code (non-zero indicates failure).
 * @ingroup http
 */
void HttpRequest::setParseErrorCode(int error) {
    _parseError = error;
}

/**
 * @brief Sets the matched server index.
 * @param index Index into the server list.
 * @ingroup http
 */
void HttpRequest::setMatchedServerIndex(int index) {
    _matchedServerIndex = index;
}

/**
 * @brief Sets the normalized Host header value.
 * @param host Host string (lowercased).
 * @ingroup http
 */
void HttpRequest::setHost(const std::string& host) {
    _host = host;
}

//=== Predicates ==========================================================

/**
 * @brief Checks whether the given header exists.
 *
 * @param key Header name to look up.
 * @return `true` if the header is present, otherwise `false`.
 * @ingroup http
 */
bool HttpRequest::hasHeader(const std::string& key) const {
    return _headers.find(key) != _headers.end();
}
