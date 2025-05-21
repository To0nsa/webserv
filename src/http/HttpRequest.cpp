/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:31:58 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/21 11:14:17 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpRequest.hpp"
#include <algorithm>
#include <iostream>
#include <set>
#include <sstream>

HttpRequest::HttpRequest(void) {
}

HttpRequest::~HttpRequest(void) {
}

void HttpRequest::printRequest() const {
    std::cout << "===== Incoming HTTP Request =====" << std::endl;
    std::cout << _method << " " << _path << " " << _version << std::endl;

    std::cout << "----- Headers -----" << std::endl;
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
         it != _headers.end(); ++it) {
        std::cout << it->first << ": " << it->second << std::endl;
    }

    if (!_body.empty()) {
        std::cout << "----- Body -----" << std::endl;
        std::cout << _body << std::endl;
    }

    std::cout << "===============================" << std::endl;
}

bool HttpRequest::parseRequestLine(const std::string& line) {
    size_t method_end = line.find(' ');
    size_t path_end   = line.find(' ', method_end + 1);
    if (method_end == std::string::npos || path_end == std::string::npos)
        return false;

    _method = line.substr(0, method_end);
    std::transform(_method.begin(), _method.end(), _method.begin(), ::toupper);

    _version = line.substr(path_end + 1);
    _version.erase(_version.find_last_not_of("\r\n") + 1);

    std::string full_uri = line.substr(method_end + 1, path_end - method_end - 1);
    size_t      qmark    = full_uri.find('?');
    if (qmark != std::string::npos) {
        _path  = full_uri.substr(0, qmark);
        _query = full_uri.substr(qmark + 1);
    } else {
        _path = full_uri;
        _query.clear();
    }

    return true;
}

bool HttpRequest::parseHeaders(std::istream& stream) {
    std::string line;
    while (std::getline(stream, line) && !line.empty() && line != "\r") {
        size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        _headers[key] = value;
    }

    return true;
}

void HttpRequest::parseBody(std::istream& stream) {
    std::string line;
    while (std::getline(stream, line)) {
        _body += line + "\n";
    }

    if (!_body.empty() && _body.back() == '\n') {
        _body.pop_back();
    }
}

bool HttpRequest::parse(const std::string& raw_request) {
    std::istringstream stream(raw_request);
    std::string        line;

    if (!std::getline(stream, line) || line.empty())
        return false;

    if (!parseRequestLine(line))
        return false;

    if (!parseHeaders(stream))
        return false;

    parseBody(stream);
    return true;
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
    const std::string upperKey = toUpper(key);
    static const std::string                           empty = "";
    std::map<std::string, std::string>::const_iterator it    = _headers.find(upperKey);
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
    return /* _uri; */_query;
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
    _headers[upperKey] = value;
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
