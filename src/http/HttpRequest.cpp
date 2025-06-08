/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:31:58 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/09 00:04:41 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpRequest.hpp"
#include "utils/stringUtils.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>

HttpRequest::HttpRequest(void) {
    _matchedServerIndex = 0; // Default to the first server
}

HttpRequest::~HttpRequest(void) {
}

void HttpRequest::printRequest() const {
    // Header
    std::cout << "\n===== HTTP REQUEST =====\n"
              << "Method:  " << _method << "\n"
              << "Path:    " << _path << "\n"
              << "Query:   " << (_query.empty() ? "(none)" : _query) << "\n"
              << "Version: " << _version << "\n\n";

    // Headers
    std::cout << "----- Headers -----\n";
    if (_headers.empty()) {
        std::cout << "  (none)\n";
    } else {
        for (const auto& [key, value] : _headers) {
            std::cout << "  " << std::left << std::setw(20) << key << ": " << value << "\n";
        }
    }

    // Body
    std::cout << "\n------ Body ------\n";
    if (_body.empty()) {
        std::cout << "  (empty)\n";
    } else {
        // Check if all chars are printable
        bool allPrint = std::all_of(_body.begin(), _body.end(), [](char c) {
            return std::isprint(static_cast<unsigned char>(c));
        });

        if (!allPrint) {
            std::cout << "  (binary data)\n";
        } else {
            // Truncate to 100 chars
            std::size_t showLen = std::min<std::size_t>(_body.size(), 100);
            std::string snippet = _body.substr(0, showLen);
            std::cout << "  " << snippet;
            if (_body.size() > showLen) {
                std::cout << "... (+" << (_body.size() - showLen) << " more)";
            }
            std::cout << "\n";
        }
    }

    // Footer
    std::cout << "=======================\n";
}

const std::string& HttpRequest::getMethod(void) const {
    return (_method);
}

const std::string& HttpRequest::getPath(void) const {
    return (_path);
}

const std::string& HttpRequest::getVersion(void) const {
    return (_version);
}

const std::string& HttpRequest::getHeader(const std::string& key) const {
    const std::string                                  upperKey = toUpper(key);
    static const std::string                           empty    = "";
    std::map<std::string, std::string>::const_iterator it       = _headers.find(upperKey);
    if (it != _headers.end())
        return (it->second);
    return (empty);
}

const std::map<std::string, std::string>& HttpRequest::getHeaders() const {
    return _headers;
}

const std::string& HttpRequest::getBody(void) const {
    return (_body);
}

std::size_t HttpRequest::getContentLength(void) const {
    return _contentLength;
}

const std::string& HttpRequest::getQuery() const {
    return _query;
}

void HttpRequest::setMethod(const std::string& method) {
    _method = method;
}

void HttpRequest::setPath(const std::string& path) {
    _path = path;
}

void HttpRequest::setVersion(const std::string& version) {
    _version = version;
}

void HttpRequest::setHeader(const std::string& key, const std::string& value) {
    const std::string upperKey = toUpper(key);
    _headers[upperKey]         = value;
}

void HttpRequest::setBody(const std::string& body) {
    _body = body;
}

void HttpRequest::setContentLength(size_t len) {
    _contentLength = len;
}

void HttpRequest::setUrl(const Url& url) {
    _url = url;
}

void HttpRequest::setQuery(const std::string& query) {
    _query = query;
}

bool HttpRequest::hasHeader(const std::string& key) const {
    return _headers.find(key) != _headers.end();
}

void HttpRequest::setParseErrorCode(int error) {
    _parseError = error;
}

int HttpRequest::getParseErrorCode(void) const {
    return _parseError;
}

void HttpRequest::setMatchedServerIndex(int index) {
    _matchedServerIndex = index;
}

int HttpRequest::getMatchedServerIndex() const {
    return _matchedServerIndex;
}

void HttpRequest::setHost(const std::string& host) {
    _host = host;
}

const std::string& HttpRequest::getHost() const {
    return _host;
}
