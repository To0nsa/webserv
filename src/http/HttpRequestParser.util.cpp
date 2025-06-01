/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestParser.util.cpp                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ktieu <ktieu@student.hive.fi>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/01 12:38:10 by ktieu             #+#    #+#             */
/*   Updated: 2025/06/01 13:03:21 by ktieu            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpRequestParser.hpp"

namespace fs = std::filesystem;

bool HttpRequestParserUtils::isValidHttpMethodToken(const std::string& method) {
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
bool HttpRequestParserUtils::isValidPath(const std::string& rawPath) {
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
Url HttpRequestParserUtils::parseUrl(HttpRequest& req, const std::string& url) {
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

/**
 * Centralized header validation & insertion.
 * - CONTENT-LENGTH → numeric check (411) + req.setContentLength
 * - TRANSFER-ENCODING → only “chunked” (501), disallow on GET (400)
 * - CL ↔ TE conflict (400)
 * - Duplicate HOST/CONTENT-TYPE (400)
 * - Duplicate CONTENT-LENGTH: only identical allowed (400)
 * - Mergeable headers appended, others rejected (400)
 */
bool HttpRequestParserUtils::insertValidatedHeader(HttpRequest& req, const std::string& key, const std::string& value,
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


bool HttpRequestParserUtils::isChunkedBodyComplete(const std::string& bodyPart) {
    // 1. Locate start of trailer section: must contain 0\r\n
    std::size_t zeroPos = bodyPart.find("0\r\n");
    if (zeroPos == std::string::npos)
        return false;

    // 2. Look for the CRLF that ends the trailer section
    std::size_t trailerEnd = bodyPart.find("\r\n\r\n", zeroPos);
    return trailerEnd != std::string::npos;
}

void HttpRequestParserUtils::chunkReqHandler(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
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
    errorCode = 0;
    consumedBytes += local;
}
