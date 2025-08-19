/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:31:10 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/19 09:04:29 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    HttpRequest.hpp
 * @brief   Immutable-ish HTTP/1.x request model with header map and parsed URL.
 *
 * @ingroup http
 *
 * @details
 * Represents a single HTTP/1.x request after (or while) being parsed by
 * @ref HttpRequestParser. The object stores:
 *  - Request line fields: method, path (raw target), and HTTP version.
 *  - Header fields in a case-preserving map (lookups are done via @ref hasHeader
 *    and @ref getHeader).
 *  - Optional message body and the tracked @ref _contentLength.
 *  - A parsed @ref Url object and extracted query string (text after `?`).
 *  - A parser status code (@ref _parseError) and the selected virtual server
 *    index (@ref _matchedServerIndex) for routing (see `requestRouter.*`).
 *  - The resolved `Host` header value for vhost matching.
 *
 * Typical lifecycle in this codebase:
 *  1. Bytes arrive on a socket and are parsed into @ref HttpRequest
 *     by `HttpRequestParser.*`.
 *  2. `requestRouter.*` sets @ref setMatchedServerIndex and may refine path/URL.
 *  3. Method handlers (`handleGet.cpp`, `handlePost.cpp`, `handleDelete.cpp`,
 *     plus `handleMultipartForm.cpp` / `handleCgi.*`) consume this object.
 *  4. `responseBuilder.*` uses fields (method, headers, body, URL) to craft an
 *     @ref HttpResponse.
 *
 * @par Invariants
 *  - If @ref getParseErrorCode returns non‑zero, the request is considered
 *    malformed and the server should generate an appropriate error response.
 *  - @ref getContentLength equals the numeric value derived from the
 *    `Content-Length` header if present; handlers must validate coherence
 *    with @ref getBody.
 *  - @ref getQuery is the substring of the request-target after `?`
 *    (empty if absent). The raw @ref getPath is not percent-decoded.
 *
 * @note This type is not thread-safe; owning code must synchronize externally.
 * @see  HttpRequestParser, Url, HttpResponse, responseBuilder, requestRouter,
 *       handleGet, handlePost, handleDelete, handleCgi
 */

#pragma once

#include "http/Url.hpp" // for Url
#include <cstddef>      // for size_t
#include <map>          // for map
#include <string>       // for string

/**
 * @brief Encapsulates a client HTTP request.
 *
 * @details Represents the full request received from a client connection,
 *          including the request line (method, target path, and protocol version),
 *          headers, body payload, and parsed URL components.
 *
 *          The class provides read/write accessors for all relevant fields,
 *          error codes for parse validation, and utility methods such as
 *          `printRequest()` for debugging. It is the central data structure
 *          produced by the @ref HttpRequestParser and consumed by the router
 *          and method handlers during request processing.
 *
 * @ingroup http
 */
class HttpRequest {
  private:
    //=== Data ================================================================

    std::string                        _method;  ///< Request method (GET, POST, etc.)
    std::string                        _path;    ///< Request path (normalized URI path)
    std::string                        _version; ///< HTTP version string (e.g. HTTP/1.1)
    std::map<std::string, std::string> _headers; ///< Request headers (case-normalized keys)
    std::string                        _body;    ///< Request body payload
    std::size_t                        _contentLength{0}; ///< Declared Content-Length, if present
    std::string                        _query;            ///< Query string (after '?')
    Url                                _url;              ///< Fully parsed URL (scheme, host, etc.)
    int                                _parseError{0};    ///< Parse error code (0 if none)
    int                                _matchedServerIndex; ///< Index of matched server block
    std::string                        _host;               ///< Normalized host from request

  public:
    //=== Construction & Special Members =====================================

    /** @name Construction & special members */
    ///@{
    HttpRequest(void);
    ~HttpRequest(void);
    ///@}

    //=== Debug & Utilities ===================================================

    /** @name Debug & utilities */
    ///@{
    /** @brief Prints a human-readable dump of the request (for debugging). */
    void printRequest(void) const;
    ///@}

    //=== Queries (Getters) ===================================================

    /** @name Queries (getters) */
    ///@{
    const std::string&                        getMethod(void) const;
    const std::string&                        getPath(void) const;
    const std::string&                        getVersion(void) const;
    const std::string&                        getHeader(const std::string& key) const;
    const std::map<std::string, std::string>& getHeaders() const;
    const std::string&                        getBody(void) const;
    std::size_t                               getContentLength(void) const;
    const std::string&                        getQuery() const;
    int                                       getParseErrorCode(void) const;
    int                                       getMatchedServerIndex() const;
    const std::string&                        getHost() const;
    ///@}

    //=== Mutators (Setters) ==================================================

    /** @name Mutators (setters) */
    ///@{
    void setMethod(const std::string& method);
    void setPath(const std::string& path);
    void setVersion(const std::string& version);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    void setContentLength(size_t len);
    void setUrl(const Url& url);
    void setQuery(const std::string& query);
    void setParseErrorCode(int error);
    void setMatchedServerIndex(int index);
    void setHost(const std::string& host);
    ///@}

    //=== Predicates ==========================================================

    /** @name Predicates */
    ///@{
    bool hasHeader(const std::string& key) const;
    ///@}
};
