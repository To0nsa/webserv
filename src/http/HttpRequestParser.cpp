/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestParser.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/25 10:36:15 by ktieu             #+#    #+#             */
/*   Updated: 2025/05/31 14:50:40 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpRequestParser.hpp"
#include "utils/Logger.hpp"
#include "utils/filesystemUtils.hpp"
#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <regex>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace {

/** Checks that method tokens only contain RFC-allowed characters. */
static bool isValidHttpMethodToken(const std::string& method) {
    if (method.empty())
        return false;
    for (char c : method) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (!std::isalnum(uc) && c != '!' && c != '#' && c != '$' && c != '%' && c != '&' &&
            c != '\'' && c != '*' && c != '+' && c != '-' && c != '.' && c != '^' && c != '_' &&
            c != '`' && c != '|' && c != '~') {
            return false;
        }
    }
    return true;
}

/** Validates that a normalized path neither escapes “/” nor has repeated “//”. */
static bool isValidPath(const std::string& rawPath) {
    fs::path    p(rawPath);
    std::string s = p.string();
    if (s == "/")
        return true;
    if (s.empty() || s.front() != '/')
        return false;
    if (s.find("//") != std::string::npos)
        return false;
    if (s == "/.." || s.find("/../") != std::string::npos || s.ends_with("/.."))
        return false;
    return true;
}

/** Parses a full URL (scheme, host, port, path, etc.) or throws. */
static Url parseUrl(HttpRequest& req, const std::string& url) {
    if (req.getVersion() == "HTTP/1.1" && req.getHeader("HOST").empty()) {
        throw std::invalid_argument("Missing HOST header (required in HTTP/1.1)");
    }
    Url                     res;
    static const std::regex re(
        R"((https?://)?(?:([^:@]+)(?::([^:@]*))?@)?([^:/?#]+)(?::(\d+))?(/[^?#]*)?(?:\?([^#]*))?(?:#(.*))?)");
    std::smatch m;
    if (!std::regex_match(url, m, re)) {
        throw std::invalid_argument("Invalid URL");
    }
    res.scheme   = m[1].str();
    res.user     = m[2].str();
    res.password = m[3].str();
    res.host     = m[4].str();
    res.port     = m[5].str();
    res.path     = m[6].str();
    res.query    = m[7].str();
    res.fragment = m[8].str();
    return res;
}

} // namespace

/**
 * Centralized header validation & insertion.
 * - CONTENT-LENGTH → numeric check (411) + req.setContentLength
 * - TRANSFER-ENCODING → only “chunked” (501), disallow on GET (400)
 * - CL ↔ TE conflict (400)
 * - Duplicate HOST/CONTENT-TYPE (400)
 * - Duplicate CONTENT-LENGTH: only identical allowed (400)
 * - Mergeable headers appended, others rejected (400)
 */
bool insertValidatedHeader(HttpRequest& req, const std::string& key, const std::string& value,
                           int& errorCode) {
    std::string normKey = toUpper(key);

    // Invalid or empty header name
    if (normKey.empty() || std::any_of(normKey.begin(), normKey.end(), [](char c) {
            unsigned char uc = static_cast<unsigned char>(c);
            return uc <= 0x1F || uc == 0x7F;
        })) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Invalid or empty header name: " + normKey);
        errorCode = 400;
        return false;
    }

    // Empty value
    if (value.empty()) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Header missing value: " + normKey);
        errorCode = 400;
        return false;
    }

    bool first = !req.hasHeader(normKey);

    // — CONTENT-LENGTH: numeric, setContentLength, errorCode=411 on format errors
    if (normKey == "CONTENT-LENGTH") {
        if (!std::all_of(value.begin(), value.end(),
                         [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Invalid Content-Length value: " + value);
            errorCode = 411;
            return false;
        }
        unsigned long long len;
        try {
            len = std::stoull(value);
        } catch (...) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Invalid Content-Length value: " + value);
            errorCode = 411;
            return false;
        }
        req.setContentLength(len);
    }

    // — EXPECT: reject 100-continue (RFC 7231 §5.1.1)
    if (normKey == "EXPECT") {
        std::string lower = toLower(value);
        if (lower == "100-continue") {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Unsupported Expect header: " + value);
            errorCode = 417;
            return false;
        }
    }

    // — TRANSFER-ENCODING: only “chunked” (501), disallowed on GET (400)
    if (normKey == "TRANSFER-ENCODING") {
        std::string lower = toLower(value);
        if (lower != "chunked") {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Unsupported Transfer-Encoding: " + lower);
            errorCode = 501;
            return false;
        }
        if (req.getMethod() == "GET") {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Chunked Transfer-Encoding not allowed for GET");
            errorCode = 400;
            return false;
        }
    }

    // — First-time CL↔TE conflict check (400)
    if (first) {
        if ((normKey == "CONTENT-LENGTH" && !req.getHeader("TRANSFER-ENCODING").empty()) ||
            (normKey == "TRANSFER-ENCODING" && req.getContentLength() > 0)) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Both Content-Length and Transfer-Encoding present");
            errorCode = 400;
            return false;
        }
    }

    // — Duplicate handling
    static const std::set<std::string> nonMergeable = {
        "HOST", "CONTENT-LENGTH", "CONTENT-TYPE", "TRANSFER-ENCODING", "EXPECT", "CONNECTION"};

    if (!first) {
        if (nonMergeable.count(normKey)) {
            if (normKey == "CONTENT-LENGTH") {
                if (req.getHeader(normKey) != value) {
                    Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                                    "Conflicting Content-Length headers");
                    errorCode = 400;
                    return false;
                }
                return true; // identical CL is OK
            }
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Duplicate " + normKey + " header");
            errorCode = 400;
            return false;
        }

        // RFC 7230 §3.2.2: mergeable by default
        req.setHeader(normKey, req.getHeader(normKey) + ", " + value);
    } else {
        req.setHeader(normKey, value);
    }

    return true;
}

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

    if (!isValidHttpMethodToken(method)) {
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

        if (!insertValidatedHeader(req, key, value, errorCode))
            return false;
    }

    // — Build URL
    if (req.getVersion() == "HTTP/1.1" || !req.getHeader("HOST").empty()) {
        try {
            Url url = parseUrl(req, req.getHeader("HOST") + req.getPath());
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

bool isChunkedBodyComplete(const std::string& bodyPart) {
    // 1. Locate start of trailer section: must contain 0\r\n
    std::size_t zeroPos = bodyPart.find("0\r\n");
    if (zeroPos == std::string::npos)
        return false;

    // 2. Look for the CRLF that ends the trailer section
    std::size_t trailerEnd = bodyPart.find("\r\n\r\n", zeroPos);
    return trailerEnd != std::string::npos;
}

void chunkReqHandler(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                     int& errorCode, std::size_t& consumedBytes) {
    std::istringstream stream(bodyPart);
    std::string        line;
    std::string        fullBody;
    std::size_t        total = 0, local = 0;

    while (std::getline(stream, line)) {
        local += line.size() + 1;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::size_t chunkSize;
        try {
            chunkSize = std::stoul(line, nullptr, 16);
        } catch (...) {
            errorCode = 400;
            consumedBytes += local;
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Invalid chunk size format");
            return;
        }

        if (chunkSize == 0) {
            // consume the final CRLF after the 0–chunk
            std::getline(stream, line);
            local += line.size() + 1;

            // reject ANY trailers (we don’t support them) ───
            std::string trailer;
            while (std::getline(stream, trailer)) {
                local += trailer.size() + 1;
                // an empty line ends the trailer section
                if (trailer.empty())
                    break;
                // any non-empty trailer header is unsupported
                errorCode = 400;
                consumedBytes += local;
                Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                                "Unsupported trailer header: " + trailer);
                return;
            }

            break;
        }

        if (total + chunkSize > clientMaxBodySize) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Exceeded max body size in chunked transfer");
            errorCode = 413;
            consumedBytes += local + chunkSize;
            return;
        }

        std::string data(chunkSize, '\0');
        stream.read(&data[0], chunkSize);
        std::streamsize got = stream.gcount();
        local += got;
        fullBody.append(data, 0, got);
        total += got;

        std::getline(stream, line);
        local += line.size() + 1;
    }

    req.setBody(fullBody);
    req.setContentLength(fullBody.size());
    errorCode = 0;
    consumedBytes += local;
}

bool parseReqBody(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                  int& errorCode, std::size_t& consumedBytes) {
    const std::string& te = req.getHeader("TRANSFER-ENCODING");

    if (te == "chunked") {
        if (!isChunkedBodyComplete(bodyPart)) {
            Logger::logFrom(LogLevel::INFO, "HttpRequestParser",
                            "Incomplete chunked body, waiting for more");
            errorCode = 0;
            return false;
        }
        chunkReqHandler(req, bodyPart, clientMaxBodySize, errorCode, consumedBytes);
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

    if (!isValidPath(req.getPath())) {
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
