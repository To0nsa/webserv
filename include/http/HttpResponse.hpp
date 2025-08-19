/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 10:55:37 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/19 09:13:33 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    HttpResponse.hpp
 * @brief   Declares the HttpResponse class.
 *
 * @details Represents an outgoing HTTP response, including status line,
 *          headers, body payload, and optional file-backed or CGI-generated
 *          content. Provides setters for constructing responses and utilities
 *          such as `toHttpString()` for serialization to the raw wire format.
 *
 *          This class is produced by response builders (see @ref ResponseBuilder),
 *          handlers (GET/POST/DELETE/CGI), and consumed by the networking layer
 *          when writing responses back to clients.
 *
 * @ingroup http
 */

#pragma once

#include <iosfwd> // for streamsize
#include <map>    // for map
#include <string> // for string

/**
 * @brief Represents an HTTP response to be sent back to the client.
 *
 * @details Encapsulates all elements of an HTTP response:
 *          - **Status line**: numeric status code and reason phrase.
 *          - **Headers**: arbitrary key–value pairs such as `Content-Type` or `Content-Length`.
 *          - **Body**: inline string payload, file-backed content, or CGI-generated output.
 *          - **Metadata**: HTTP version, connection management, and offsets for CGI output.
 *
 * Provides a clear API for constructing responses via setters, and utilities
 * for serializing them (`toHttpString`) or checking response type (file-backed
 * vs. CGI temporary file).
 *
 * Typical workflow:
 * - Built by method handlers (GET/POST/DELETE, CGI) or the @ref ResponseBuilder.
 * - Sent by the networking layer after request handling.
 *
 * @ingroup http
 */
class HttpResponse {
  private:
    //=== Data ================================================================

    int         _status_code;                    ///< Numeric HTTP status code (e.g., 200, 404).
    std::string _status_message;                 ///< Reason phrase (e.g., "OK", "Not Found").
    std::map<std::string, std::string> _headers; ///< Response headers (case-preserving keys).
    std::string                        _body;    ///< Response body payload (may be empty).
    std::string     _http_version;               ///< Request HTTP version for keep-alive logic.
    std::string     _connection_header;          ///< Client "Connection" header snapshot.
    std::string     _file_path;                  ///< If set, path to file-backed body to stream.
    std::string     _cgi_temp_file;              ///< Temp file produced by CGI (for cleanup).
    std::streamsize _cgiBodyOffset; ///< Byte offset where CGI body begins in temp file.

  public:
    //=== Construction & Special Members =====================================

    /** @name Construction & special members */
    ///@{
    HttpResponse(void);
    ~HttpResponse(void);
    HttpResponse(const HttpResponse& other)            = default;
    HttpResponse& operator=(const HttpResponse& other) = default;
    ///@}

    //=== Mutators (Setters) ==================================================

    /** @name Mutators (setters) */
    ///@{
    void setStatus(int code, const std::string& message);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    void setRequestMeta(const std::string& version, const std::string& conn);
    void setFilePath(const std::string& path);
    void setCgiBodyOffset(std::streamsize offset);
    void setCgiTempFile(const std::string& temp_file);
    ///@}

    //=== Queries (Getters) ===================================================

    /** @name Queries (getters) */
    ///@{
    std::string                               toHttpString(void) const;
    bool                                      isConnectionClose(void) const;
    const std::string&                        getFilePath() const;
    bool                                      isFileResponse() const;
    int                                       getStatusCode(void) const;
    const std::string&                        getStatusMessage(void) const;
    const std::map<std::string, std::string>& getHeaders(void) const;
    std::streamsize                           getCgiBodyOffset() const;
    const std::string&                        getCgiTempFile() const;
    bool                                      isCgiTempFile() const;
    ///@}
};
