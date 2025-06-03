/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestParser.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ktieu <ktieu@student.hive.fi>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/25 10:36:15 by ktieu             #+#    #+#             */
/*   Updated: 2025/06/03 10:43:15 by ktieu            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpRequestParser.hpp"

namespace fs = std::filesystem;

bool parseReqHeader(HttpRequest& req, const std::string& headerPart, int& errorCode) {
    std::istringstream stream(headerPart);
    std::string        line;

    // — Parse start line
    if (!std::getline(stream, line) || line.empty()) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Empty or missing request start line");
        errorCode = 400;
        return false;
    }
    size_t sp1 = line.find(' ');
    size_t sp2 = sp1 == std::string::npos ? std::string::npos : line.find(' ', sp1 + 1);
    size_t sp3 = sp2 == std::string::npos ? std::string::npos : line.find(' ', sp2 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos || sp3 != std::string::npos) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Malformed request line (must have exactly two spaces)");
        errorCode = 400;
        return false;
    }

    std::string method    = line.substr(0, sp1);
    std::string rawTarget = line.substr(sp1 + 1, sp2 - sp1 - 1);
    std::string version   = trim(line.substr(sp2 + 1));

    if (!HttpRequestParserUtils::isValidHttpMethodToken(method)) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Method Not Allowed: " + method);
        errorCode = 400;
        return false;
    }
    if (method.empty() || rawTarget.empty() || version.empty()) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Invalid request start line fields");
        errorCode = 400;
        return false;
    }

    // — Decode and validate path
    std::string decoded;
    try {
        decoded = decodePercentEncoding(rawTarget);
    } catch (const std::exception& e) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", e.what());
        errorCode = 400;
        return false;
    }
    for (unsigned char c : decoded) {
        if (std::iscntrl(c)) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Path contains control characters");
            errorCode = 400;
            return false;
        }
    }

    // — Split path/query
    std::string pathOnly = decoded, query;
    size_t      qpos     = decoded.find('?');
    if (qpos != std::string::npos) {
        pathOnly = decoded.substr(0, qpos);
        query    = decoded.substr(qpos + 1);
    }

    static constexpr size_t MAX_URI_LEN = 2048;
    if (pathOnly.size() > MAX_URI_LEN) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Request-URI Too Long: " + pathOnly);
        errorCode = 414;
        return false;
    }

    // — Initialize request
    req = HttpRequest();
    req.setMethod(method);
    if (pathOnly.find("..") != std::string::npos) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Rejected traversal attempt: " + pathOnly);
        errorCode = 403;
        return false;
    }
    std::string norm = normalizePath(pathOnly);
    if (norm.empty()) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Path escapes root: " + pathOnly);
        errorCode = 403;
        return false;
    }
    req.setPath(norm);
    req.setQuery(query);
    req.setVersion(version);

    if (version != "HTTP/1.0" && version != "HTTP/1.1") {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Invalid HTTP version: " + version);
        errorCode = 505;
        return false;
    }

    // — Parse headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 >= line.size()) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Malformed header line: " + line);
            errorCode = 400;
            return false;
        }

        std::string key = line.substr(0, colon);
        if (key.empty() || key.find_first_of(" \t") != std::string::npos) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Invalid header name: " + key);
            errorCode = 400;
            return false;
        }
        std::string value = line.substr(colon + 1);
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        if (!HttpRequestParserUtils::insertValidatedHeader(req, key, value, errorCode))
            return false;
    }

    // — Build URL
    if (req.getVersion() == "HTTP/1.1" || !req.getHeader("HOST").empty()) {
        try {
            Url url = HttpRequestParserUtils::parseUrl(req, req.getHeader("HOST") + req.getPath());
            req.setUrl(url);
        } catch (const std::exception& e) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", e.what());
            errorCode = 400;
            return false;
        }
    } else {
        Url dummy;
        dummy.path = req.getPath();
        dummy.host = req.getHeader("HOST");
        req.setUrl(dummy);
        Logger::logFrom(LogLevel::INFO, "HttpRequestParser", "Using fallback Url for HTTP/1.0");
    }

    return true;
}

bool parseReqBody(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                  int& errorCode, std::size_t& consumedBytes) {
    const std::string& te = req.getHeader("TRANSFER-ENCODING");

    if (te == "chunked") {
        if (!HttpRequestParserUtils::isChunkedBodyComplete(bodyPart)) {
            Logger::logFrom(LogLevel::INFO, "HttpRequestParser",
                            "Incomplete chunked body, waiting for more");
            errorCode = 0;
            return false;
        }
        HttpRequestParserUtils::chunkReqHandler(req, bodyPart, clientMaxBodySize, errorCode, consumedBytes);
        return errorCode == 0;
    }

    std::size_t len = req.getContentLength();
    if (len > 0 && len >= clientMaxBodySize) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Exceeded max body size in non-chunked transfer");
        errorCode = 413;
        consumedBytes += bodyPart.size();
        return false;
    }

    if (bodyPart.size() < len) {
        Logger::logFrom(LogLevel::INFO, "HttpRequestParser", "Incomplete body, waiting for more");
        errorCode = 0;
        return false;
    }

    req.setBody(bodyPart.substr(0, len));
    consumedBytes += len;
    errorCode = 0;
    return true;
}

bool validateReq(HttpRequest& req, int& errorCode) {
    static const std::set<std::string> methods = {"GET", "POST", "DELETE"};

    if (!methods.count(req.getMethod())) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Method Not Allowed: " + req.getMethod());
        errorCode = 501;
        return false;
    }

    if (!HttpRequestParserUtils::isValidPath(req.getPath())) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Invalid request path: " + req.getPath());
        errorCode = 403;
        return false;
    }

    // Connection header
    std::string conn = req.getHeader("CONNECTION");
    if (!conn.empty()) {
        std::string lower = toLower(conn);
        if (lower != "keep-alive" && lower != "close") {
            req.setHeader("CONNECTION", "close");
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Invalid Connection header value: " + conn);
            errorCode = 400;
            return false;
        }
    }

    // POST → must have supported Content-Type
    if (req.getMethod() == "POST") {

        if (req.getContentLength() == 0 && req.getHeader("TRANSFER-ENCODING").empty()) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "POST without Content-Length or Transfer-Encoding");
            errorCode = 411;
            return false;
        }

        std::string ct = req.getHeader("CONTENT-TYPE");
        if (ct.empty()) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Missing Content-Type for POST");
            errorCode = 415;
            return false;
        }

        static const std::set<std::string> validTypes = {"application/x-www-form-urlencoded",
                                                         "multipart/form-data",
                                                         "text/plain",
                                                         "application/json",
                                                         "application/octet-stream",
                                                         "test/file"};

        // Accept type with optional parameters (e.g. multipart/form-data; boundary=...)
        std::string lowerCt = toLower(ct);
        bool        valid   = false;
        for (const std::string& type : validTypes) {
            if (lowerCt.rfind(type, 0) == 0) { // prefix match
                valid = true;
                break;
            }
        }

        if (!valid) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Unsupported Content-Type for POST: " + ct);
            errorCode = 415;
            return false;
        }
    }

    return true;
}

bool HttpRequestParser::parse(HttpRequest& req, const std::string& raw_req,
                              std::size_t clientMaxBodySize, int& errorCode,
                              std::size_t& consumedBytes) {
    size_t pos = raw_req.find("\r\n\r\n");
    if (pos == std::string::npos) {
        errorCode = 0;
        Logger::logFrom(LogLevel::INFO, "HttpRequestParser", "Incomplete header, waiting for more");
        return false;
    }

    std::string headerPart = raw_req.substr(0, pos);
    std::string bodyPart   = raw_req.substr(pos + 4);
    consumedBytes          = pos + 4;

    if (!parseReqHeader(req, headerPart, errorCode))
        return false;
    if (!validateReq(req, errorCode))
        return false;
    if (!parseReqBody(req, bodyPart, clientMaxBodySize, errorCode, consumedBytes))
        return false;

    return true;
}
