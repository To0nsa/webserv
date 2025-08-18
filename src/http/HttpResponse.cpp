/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 10:56:54 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/18 12:05:43 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpResponse.hpp"
#include <algorithm> // for transform
#include <ctype.h>   // for tolower
#include <sstream>   // for basic_ostream, operator<<, basic_stringstream
#include <utility>   // for pair
#include <set>       // for set

HttpResponse ::HttpResponse(void) {
    _status_code    = 200;
    _status_message = "OK";
    _cgiBodyOffset  = 0;
}

HttpResponse ::~HttpResponse(void) {
}

void HttpResponse ::setStatus(int code, const std::string& message) {
    _status_code    = code;
    _status_message = message;
}

void HttpResponse ::setHeader(const std::string& key, const std::string& value) {
    _headers[key] = value;
}

void HttpResponse ::setBody(const std::string& body) {
    _body = body;
}

/**
 * @brief Stores HTTP version and Connection header from the request.
 *
 * @details This metadata is required to determine whether the connection should be
 * kept alive or closed after the response is sent. The HTTP version and the client's
 * `Connection` header together define the default persistence behavior according to
 * RFC 7230 §6.3. This method must be called before sending the response to ensure
 * proper behavior in `isConnectionClose()`.
 *
 * @param version The HTTP version from the client's request (e.g., "HTTP/1.1").
 * @param conn    The value of the Connection header from the client's request.
 */
void HttpResponse::setRequestMeta(const std::string& version, const std::string& conn) {
    _http_version      = version;
    _connection_header = conn;
}

/**
 * @brief Determines whether the connection should be closed after the response.
 *
 * @details This logic properly accounts for the HTTP version and the client's
 * Connection header to implement persistent connections correctly.
 *
 * Unlike the previous version, which only checked if `Connection: close` was present,
 * this method enforces the default semantics of each protocol version:
 * - HTTP/1.1 assumes keep-alive unless explicitly closed.
 * - HTTP/1.0 assumes close unless explicitly kept alive.
 *
 * This behavior is compliant with RFC 7230 §6.3 and avoids premature connection
 * termination when clients do not send a `Connection` header.
 *
 * @return `true` if the connection should be closed, `false` to keep it alive.
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

std::string HttpResponse ::toHttpString(void) const {
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

void HttpResponse::setFilePath(const std::string& path) {
    _file_path = path;
}

const std::string& HttpResponse::getFilePath() const {
    return _file_path;
}

bool HttpResponse::isFileResponse() const {
    return !_file_path.empty();
}

int HttpResponse::getStatusCode(void) const {
    return _status_code;
}

const std::string& HttpResponse::getStatusMessage(void) const {
    return _status_message;
}

const std::map<std::string, std::string>& HttpResponse::getHeaders(void) const {
    return _headers;
}

void HttpResponse::setCgiBodyOffset(std::streamsize offset) {
    _cgiBodyOffset = offset;
}

std::streamsize HttpResponse::getCgiBodyOffset() const {
    return _cgiBodyOffset;
}

void HttpResponse::setCgiTempFile(const std::string& temp_file) {
    _cgi_temp_file = temp_file;
}
const std::string& HttpResponse::getCgiTempFile() const {
    return _cgi_temp_file;
}

bool HttpResponse::isCgiTempFile() const {
    return !_cgi_temp_file.empty();
}
