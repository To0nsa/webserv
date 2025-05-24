
#include "http/HttpRequestParser.hpp"
#include <algorithm>
#include <iostream>
#include <ranges>
#include <regex>
#include <sstream>
#include <set>

bool parseReqHeader(HttpRequest& req, const std::string& headerPart, int& errorCode);
bool parseReqBody(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                  int& errorCode, std::size_t& consumedBytes);
bool isChunkedBodyComplete(const std::string& bodyPart);
Url parseUrl(HttpRequest& req, const std::string& url);
bool validateReq(HttpRequest& req, int& errorCode);
bool isValidHeader(std::string& key);

bool HttpRequestParser::parse(HttpRequest& req, const std::string& raw_req,
                              std::size_t clientMaxBodySize, int& errorCode, std::size_t& consumedBytes) {
    std::size_t headerEndPos = raw_req.find("\r\n\r\n");
    if (headerEndPos == std::string::npos) {
        errorCode = 0; // Incomplete request
        std::cout << "[INFO] HttpRequestParser: Incomplete header, server reads again" << std::endl;
        return false;
    }
    std::string headerPart = raw_req.substr(0, headerEndPos);
    std::string bodyPart   = raw_req.substr(headerEndPos + 4);

	consumedBytes = headerEndPos + 4;
    if (!parseReqHeader(req, headerPart, errorCode))
        return false;
    if (!validateReq(req, errorCode))
        return false;
    if (!parseReqBody(req, bodyPart, clientMaxBodySize, errorCode, consumedBytes))
        return false;
    return true;
}

bool parseReqHeader(HttpRequest& req, const std::string& headerPart, int& errorCode) {
    std::istringstream stream(headerPart);
    std::string        line;
    if (!std::getline(stream, line) || line.empty()) {
		std::cerr << "[ERROR] HttpRequestParser: Empty line" << std::endl;
        errorCode = 400;
        return false;
    }
    std::istringstream requestLineStream(line);
    std::string        method, path, version;
    requestLineStream >> method >> path >> version;

    if (method.empty() || path.empty() || version.empty()) {
		std::cerr << "[ERROR] HttpRequestParser: Invalid request startline" << std::endl;
        errorCode = 400;
        return false;
    }

    req = HttpRequest();
    req.setMethod(method);
    req.setPath(path);
    req.setVersion(version);

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            continue; // skip invalid headers
        }
        std::string key   = toUpper(line.substr(0, colonPos));
        std::string value = line.substr(colonPos + 1);
        key.erase(key.find_last_not_of(" \t\r\n") + 1);     // remove trailing whitespace
        value.erase(0, value.find_first_not_of(" \t\r\n")); // remove leading whitespace

        if (!isValidHeader(key)) {
            std::cerr << "[ERROR] HttpRequestParser: Invalid header key: " << key << std::endl;
            errorCode = 400;
            return false;
        }
        if ((key == "TRANSFER-ENCODING") && value == "chunked" && req.getMethod() == "GET") {
            std::cerr << "[ERROR] HttpRequestParser: Chunked transfer encoding is not allowed in GET requests" << std::endl;
            errorCode = 400;
            return false;
        }
        if (key == "CONTENT-LENGTH") {
            if (value.empty() || !std::all_of(value.begin(), value.end(), [](char c) {
                    return std::isdigit(static_cast<unsigned char>(c));
                })) {
                std::cerr << "[ERROR] HttpRequestParser: Invalid Content-Length value:" << std::endl;
                errorCode = 411;
                return false;
            }

            try {
                req.setContentLength(std::stoull(value));
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] HttpRequestParser: Invalid Content-Length value:" << e.what() << std::endl;
                errorCode = 411;
                return false;
            }
        }

        // Handling duplicated header
        auto existing = req.getHeader(key);
        if (!existing.empty()) {
            req.setHeader(key, existing + ", " + value);
        } else {
            req.setHeader(key, value);
        }
    }

    try {
        Url url = parseUrl(req, req.getHeader("HOST") + req.getPath());
        req.setUrl(url);
    } catch (const std::exception& e) {
        errorCode = 400;
        std::cerr << "[ERROR] HttpRequestParser: " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool isChunkedBodyComplete(const std::string& bodyPart) {
    return bodyPart.find("0\r\n\r\n") != std::string::npos;
}

void chunkReqHandler(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                     int& errorCode, std::size_t& consumedBytes) {
    std::istringstream stream(bodyPart);
    std::string        chunkLine;
    std::string        fullBody;
    std::size_t        totalSize = 0;
	std::size_t        localConsumed = 0;

    while (std::getline(stream, chunkLine)) {
		localConsumed += chunkLine.size() + 1; // +1 for '\n'
        if (!chunkLine.empty() && chunkLine.back() == '\r') {
            chunkLine.pop_back();
        }

        std::size_t chunkSize = 0;
        try {
            chunkSize = std::stoul(chunkLine, nullptr, 16); // Hexadecimal
        } catch (...) {
            errorCode = 400;
			consumedBytes += localConsumed;
            std::cerr << "[ERROR] HttpRequestParser: Invalid chunk size format" << std::endl;
            return;
        }

        if (chunkSize == 0) {
			// final chunk, consume \r\n
            std::string lastLine;
            std::getline(stream, lastLine);
            localConsumed += lastLine.size() + 1; // Usually just \r\n
            break; // End of chunks
        }

        if (totalSize + chunkSize > clientMaxBodySize) {
            std::cerr << "[ERROR] HttpRequestParser: Exceeded request max body size in chunked transfer" << std::endl;
			consumedBytes += localConsumed + chunkSize;
            errorCode = 413;
            return;
        }

        std::string chunkData(chunkSize, '\0');
        stream.read(&chunkData[0], chunkSize);
		std::streamsize bytesRead = stream.gcount();
        localConsumed += bytesRead;

        fullBody += chunkData;
        totalSize += bytesRead;

        // Consume trailing "\r\n" after the chunk
        std::string trailing;
        std::getline(stream, trailing);
        localConsumed += trailing.size() + 1; // for \n
    }

    req.setBody(fullBody);
    errorCode = 0;
	consumedBytes += localConsumed;
}

bool parseReqBody(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                  int& errorCode, std::size_t& consumedBytes) {
    /* if (req.getMethod() == "GET") {
        errorCode = 0;
        return true;
    } */
    const std::string& transferEncoding = req.getHeader("TRANSFER-ENCODING");

    if (transferEncoding == "chunked") {
        if (!isChunkedBodyComplete(bodyPart)) {
            std::cout << "[INFO] HttpRequestParser: Incomplete chunked body, server reads again" << std::endl;
            errorCode = 0;
            return false;
        }
        std::cout << "[DEBUG] HttpRequestParser: Handling chunked request" << std::endl;
        chunkReqHandler(req, bodyPart, clientMaxBodySize, errorCode, consumedBytes);
        return errorCode == 0;
    }

    std::size_t contentLength = req.getContentLength();
    if (contentLength >= clientMaxBodySize) {
        std::cerr << "[ERROR] HttpRequestParser: Exceeded request max body size" << std::endl;
        errorCode = 413;
		consumedBytes += bodyPart.size();
        return false;
    }

    if (bodyPart.size() < contentLength) {
        std::cout << "[INFO] HttpRequestParser: Incomplete request body, server reads again" << std::endl;
        errorCode = 0;
        return false;
    }

    std::string bodyContent = bodyPart.substr(0, contentLength);
	consumedBytes += bodyContent.size();
    req.setBody(bodyContent);

    errorCode = 0;
    return true;
}

Url parseUrl(HttpRequest& req, const std::string& url) {
    if (req.getHeader("HOST").empty()) {
        throw std::invalid_argument("Missing HOST header");
    }
    Url        res;
    std::regex urlRegex(
        R"((https?://)?(?:([^:@]+)(?::([^:@]*))?@)?([^:/?#]+)(?::(\d+))?(/[^?#]*)?(?:\?([^#]*))?(?:#(.*))?)");
    std::smatch matches;

    if (!std::regex_match(url, matches, urlRegex)) {
        throw std::invalid_argument("Invalid URL");
    }

    res.scheme   = matches[1].str();
    res.user     = matches[2].str();
    res.password = matches[3].str();
    res.host     = matches[4].str();
    res.port     = matches[5].str();
    res.path     = matches[6].str();
    res.query    = matches[7].str();
    res.fragment = matches[8].str();
    return res;
}

bool validateReq(HttpRequest& req, int& errorCode) {
    const std::set<std::string> validMethods = {"GET", "POST", "DELETE"};
    if (validMethods.find(req.getMethod()) == validMethods.end()) {
        errorCode = 405; // Method Not Allowed
		std::cerr << "[ERROR] HttpRequestParser: Invalid method" << std::endl;
        return false;
    }

    if (req.getVersion() != "HTTP/1.0" && req.getVersion() != "HTTP/1.1") {
        errorCode = 505; // HTTP Version Not Supported
		std::cerr << "[ERROR] HttpRequestParser: Invalid HTTP version" << std::endl;
        return false;
    }

    if (req.getPath()[0] != '/') {
        errorCode = 400;
        return false;
    }

    const std::string& transferEncoding = req.getHeader("TRANSFER-ENCODING");
    if (!transferEncoding.empty() && req.getContentLength() > 0) {
        errorCode = 400; // Bad Request
        std::cerr << "Conflicting Transfer-Encoding and Content-Length" << std::endl;
        return false;
    }

    // validate header keys and values

    return true;
}

bool isValidHeader(std::string& key) {
    static const std::set<std::string> validHeaders = {
        "CONTENT-TYPE",
        "CONTENT-ENCODING",
        "CONTENT-LANGUAGE",
        "CONTENT-LOCATION",
        "CONTENT-LENGTH",
        "CONTENT_RANGE",
        "TRAILER",
        "TRANSFER-ENCODING",
        "CACHE-CONTROL",
        "CONNECTION",
        "EXPECT",
        "HOST",
        "MAX-FORWARDS",
        "PRAGMA",
        "RANGE",
        "TE",
        "IF-MATCH",
        "IF-NONE-MATCH",
        "IF-MODIFIED-SINCE",
        "IF-UNMODIFIED-SINCE",
        "IF-RANGE",
        "ACCEPT",
        "ACCEPT-CHARSET",
        "ACCEPT-ENCODING",
        "ACCEPT-LANGUAGE",
        "AUTHORIZATION",
        "PROXY-AUTHORIZATION",
        "FROM",
        "REFERER",
        "USER-AGENT",
        "AGE",
        "EXPIRES",
        "DATE",
        "LOCATION",
        "RETRY-AFTER",
        "VARY",
        "WARNING",
        "ETAG",
        "LAST-MODIFIED",
        "WWW-AUTHENTICATE",
        "PROXY-AUTHENTICATE",
        "ACCEPT-RANGES",
        "ALLOW",
        "SERVER",
        "MIME_VERSION"
    };
    return validHeaders.find(key) != validHeaders.end();
}
