/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:31:58 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/07 15:03:24 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpRequest.hpp"
#include "utils/stringUtils.hpp"

#include <algorithm>
#include <iostream>
#include <set>
#include <sstream>

HttpRequest::HttpRequest(void) {
	_matchedServerIndex = 0; // Default to the first server
}

HttpRequest::~HttpRequest(void) {
}

void HttpRequest::printRequest() const {
    std::cout << "\n========== HTTP REQUEST ==========\n";
    std::cout << "Method: {" << _method << "}\n";
    std::cout << "Path: {" << _path << "}\n";
    std::cout << "Query: {" << _query << "}\n";
    std::cout << "Version: {" << _version << "}\n";

    std::cout << "------------- Headers -------------\n";
    if (_headers.empty()) {
        std::cout << "Headers: {(none)}\n";
    } else {
        for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
             it != _headers.end(); ++it) {
            std::cout << it->first << ": {" << it->second << "}\n";
        }
    }

    std::cout << "-------------- Body ---------------\n";
    /*     if (!_body.empty()) {
            std::cout << "Body: {\n" << _body << "\n}\n";
        } else {
            std::cout << "Body: {(empty)}\n";
        } */

    std::cout << "===================================\n";
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
    /* const std::string& HttpRequest::getUri(void) const { */
    return /* _uri; */ _query;
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
