/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestParser.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/25 13:09:07 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/25 14:37:28 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpRequestParser.hpp"
#include "utils/stringUtils.hpp"
#include <algorithm>
#include <iostream>
#include <ranges>
#include <regex>
#include <sstream>

//----------------------------------------
// FREE-FUNCTION PROTOTYPES
//----------------------------------------
bool parseReqHeader(HttpRequest& req, const std::string& headerPart, int& errorCode);
bool parseReqBody(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                  int& errorCode);
bool isChunkedBodyComplete(const std::string& bodyPart);
void chunkReqHandler(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                     int& errorCode);
Url  parseUrl(const std::string& url);

//----------------------------------------
// CONSTANTS
//----------------------------------------
static constexpr const char* HEADER_BODY_DELIM     = "\r\n\r\n";
static constexpr size_t      HEADER_BODY_DELIM_LEN = 4;
static constexpr const char* CHUNK_TERMINATOR      = "\r\n0\r\n\r\n";
static constexpr size_t      CHUNK_TERMINATOR_LEN  = 7;

//----------------------------------------
// CLASS METHODS
//----------------------------------------

// New overload: returns true when a full request (header+body) is parsed and
// sets 'consumed' to the total bytes read. On incomplete data, returns false
// with errorCode==0. On protocol errors, returns false with errorCode set.
bool HttpRequestParser::parse(HttpRequest& req, const std::string& raw_req,
                              std::size_t clientMaxBodySize, int& errorCode,
                              std::size_t& consumed) {
    errorCode = 0;
    consumed  = 0;

    // 1) Locate end of headers
    auto headerEndPos = raw_req.find(HEADER_BODY_DELIM);
    if (headerEndPos == std::string::npos) // header incomplete
        return false;

    size_t      headerLen  = headerEndPos + HEADER_BODY_DELIM_LEN;
    std::string headerPart = raw_req.substr(0, headerEndPos);
    std::string bodyPart   = raw_req.substr(headerLen);

    // 2) Parse the request-line & headers
    if (!parseReqHeader(req, headerPart, errorCode))
        return false;

    // 3) GET never has a body
    if (req.getMethod() == "GET") {
        consumed = headerLen;
        return true;
    }

    // 4) Grab TE once
    const auto& te = req.getHeader("TRANSFER-ENCODING");

    // 5) No CL header *and* not chunked ⇒ zero-byte POST is complete
    if (req.getHeader("CONTENT-LENGTH").empty() && te != "chunked") {
        consumed = headerLen;
        return true;
    }

    // 6) Chunked body?
    if (te == "chunked") {
        auto termPos = raw_req.find(CHUNK_TERMINATOR, headerLen);
        if (termPos == std::string::npos) // chunked body incomplete
            return false;

        size_t bodyConsumed = termPos + CHUNK_TERMINATOR_LEN - headerLen;
        chunkReqHandler(req, raw_req.substr(headerLen, bodyConsumed), clientMaxBodySize, errorCode);
        if (errorCode != 0)
            return false;

        consumed = termPos + CHUNK_TERMINATOR_LEN;
        return true;
    }

    // 7) Content-Length body
    auto len = req.getContentLength();
    if (len > clientMaxBodySize) {
        errorCode = 413;
        return false;
    }
    if (bodyPart.size() < len) // still waiting for more
        return false;

    req.setBody(bodyPart.substr(0, len));
    consumed = headerLen + len;
    return true;
}

/* // Backwards-compatible: old signature calls new and ignores consumed
bool HttpRequestParser::parse(HttpRequest& req, const std::string& raw_req,
                              std::size_t clientMaxBodySize, int& errorCode) {
    std::size_t bytes;
    return parse(req, raw_req, clientMaxBodySize, errorCode, bytes);
} */

//----------------------------------------
// INTERNAL HELPERS
//----------------------------------------

bool parseReqHeader(HttpRequest& req, const std::string& headerPart, int& errorCode) {
    std::istringstream stream(headerPart);
    std::string        line;

    // Request line
    if (!std::getline(stream, line) || line.empty()) {
        errorCode = 400;
        return false;
    }
    std::istringstream requestLine(line);
    std::string        method, rawTarget, version;
    if (!(requestLine >> method >> rawTarget >> version)) {
        errorCode = 400;
        return false;
    }

    // Initialize request
    req = HttpRequest();
    req.setMethod(method);
    req.setVersion(version);

    // Split path and query
    auto qpos = rawTarget.find('?');
    if (qpos != std::string::npos) {
        req.setPath(rawTarget.substr(0, qpos));
        req.setQuery(rawTarget.substr(qpos + 1));
    } else {
        req.setPath(rawTarget);
        req.setQuery("");
    }

    // Parse headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        auto colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        auto key   = toUpper(line.substr(0, colon));
        auto value = line.substr(colon + 1);
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t\r\n"));

        if (key == "TRANSFER-ENCODING" && value == "chunked" && req.getMethod() == "GET") {
            errorCode = 400;
            return false;
        }
        if (key == "CONTENT-LENGTH") {
            if (value.empty() || !std::ranges::all_of(value, ::isdigit)) {
                errorCode = 411;
                return false;
            }
            try {
                req.setContentLength(std::stoull(value));
            } catch (...) {
                errorCode = 411;
                return false;
            }
        }
        req.setHeader(key, value);
    }

    // Host is mandatory
    if (req.getHeader("HOST").empty()) {
        errorCode = 400;
        return false;
    }

    // Build URL for path/query validation
    try {
        Url url = parseUrl(req.getHeader("HOST") + req.getPath());
        req.setUrl(url);
    } catch (const std::exception&) {
        errorCode = 400;
        return false;
    }
    return true;
}

bool isChunkedBodyComplete(const std::string& bodyPart) {
    return bodyPart.find(CHUNK_TERMINATOR) != std::string::npos;
}

void chunkReqHandler(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                     int& errorCode) {
    std::istringstream stream(bodyPart);
    std::string        line;
    std::string        fullBody;
    std::size_t        total = 0;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::size_t chunkSize = 0;
        try {
            chunkSize = std::stoul(line, nullptr, 16);
        } catch (...) {
            errorCode = 400;
            return;
        }
        if (chunkSize == 0)
            break;
        if (total + chunkSize > clientMaxBodySize) {
            errorCode = 413;
            return;
        }
        std::string data(chunkSize, '\0');
        stream.read(data.data(), chunkSize);
        fullBody += data;
        total += chunkSize;
        std::getline(stream, line); // skip trailing CRLF
    }
    req.setBody(fullBody);
    errorCode = 0;
}

bool parseReqBody(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                  int& errorCode) {
    if (req.getMethod() == "GET") {
        errorCode = 0;
        return true;
    }
    const auto& te = req.getHeader("TRANSFER-ENCODING");
    if (te == "chunked") {
        if (!isChunkedBodyComplete(bodyPart)) {
            errorCode = 0;
            return false;
        }
        chunkReqHandler(req, bodyPart, clientMaxBodySize, errorCode);
        return errorCode == 0;
    }
    auto len = req.getContentLength();
    if (len > clientMaxBodySize) {
        errorCode = 413;
        return false;
    }
    if (bodyPart.size() < len) {
        errorCode = 0;
        return false;
    }
    req.setBody(bodyPart.substr(0, len));
    errorCode = 0;
    return true;
}

Url parseUrl(const std::string& url) {
    Url                     res;
    static const std::regex urlRx(
        R"((https?://)?(?:([^:@]+)(?::([^:@]*))?@)?([^:/?#]+)(?::(\d+))?(/[^?#]*)?(?:\?([^#]*))?(?:#(.*))?)");
    std::smatch m;
    if (!std::regex_match(url, m, urlRx))
        throw std::invalid_argument("Invalid URL");
    res.scheme   = m[1];
    res.user     = m[2];
    res.password = m[3];
    res.host     = m[4];
    res.port     = m[5];
    res.path     = m[6];
    res.query    = m[7];
    res.fragment = m[8];
    return res;
}
