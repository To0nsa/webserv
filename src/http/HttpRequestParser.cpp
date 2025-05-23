
#include "http/HttpRequestParser.hpp"
#include <algorithm>
#include <iostream>
#include <ranges>
#include <regex>
#include <sstream>
#include <set>

bool parseReqHeader(HttpRequest& req, const std::string& headerPart, int& errorCode);
bool parseReqBody(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                  int& errorCode);
bool isChunkedBodyComplete(const std::string& bodyPart);
Url parseUrl(HttpRequest& req, const std::string& url);
bool validateReq(HttpRequest& req, int& errorCode);
bool isValidHeader(std::strin& key);

bool HttpRequestParser::parse(HttpRequest& req, const std::string& raw_req,
                              std::size_t clientMaxBodySize, int& errorCode) {
    std::size_t headerEndPos = raw_req.find("\r\n\r\n");
    if (headerEndPos == std::string::npos) {
        errorCode = 0; // Incomplete request
        std::cout << "[INFO] HttpRequestParser: Incomplete header, server reads again" << std::endl;
        return false;
    }
    std::string headerPart = raw_req.substr(0, headerEndPos);
    std::string bodyPart   = raw_req.substr(headerEndPos + 4);

    if (!parseReqHeader(req, headerPart, errorCode))
        return false;
    if (!validateReq(req, errorCode))
        return false;
    if (!parseReqBody(req, bodyPart, clientMaxBodySize, errorCode))
        return false;
    return true;
}

bool parseReqHeader(HttpRequest& req, const std::string& headerPart, int& errorCode) {
    std::istringstream stream(headerPart);
    std::string        line;
    if (!std::getline(stream, line) || line.empty()) {
        errorCode = 400;
        return false;
    }
    std::istringstream requestLineStream(line);
    std::string        method, path, version;
    requestLineStream >> method >> path >> version;

    if (method.empty() || path.empty() || version.empty()) {
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
        req.setHeader(key, value);
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
    return bodyPart.find("\r\n0\r\n\r\n") != std::string::npos;
}

void chunkReqHandler(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                     int& errorCode) {
    std::istringstream stream(bodyPart);
    std::string        chunkLine;
    std::string        fullBody;
    std::size_t        totalSize = 0;

    while (std::getline(stream, chunkLine)) {
        if (!chunkLine.empty() && chunkLine.back() == '\r') {
            chunkLine.pop_back();
        }

        std::size_t chunkSize = 0;
        try {
            chunkSize = std::stoul(chunkLine, nullptr, 16); // Hexadecimal
        } catch (...) {
            errorCode = 400;
            std::cerr << "[ERROR] HttpRequestParser: Invalid chunk size format" << std::endl;
            return;
        }

        if (chunkSize == 0) {
            break; // End of chunks
        }

        if (totalSize + chunkSize > clientMaxBodySize) {
            std::cerr << "[ERROR] HttpRequestParser: Exceeded request max body size in chunked transfer" << std::endl;
            errorCode = 413;
            return;
        }

        std::string chunkData(chunkSize, '\0');
        stream.read(&chunkData[0], chunkSize);

        fullBody += chunkData;
        totalSize += chunkSize;

        // Consume trailing "\r\n" after the chunk
        std::getline(stream, chunkLine);
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
    const std::string& transferEncoding = req.getHeader("TRANSFER-ENCODING");

    if (transferEncoding == "chunked") {
        if (!isChunkedBodyComplete(bodyPart)) {
            std::cout << "[INFO] HttpRequestParser: Incomplete chunked body, server reads again" << std::endl;
            errorCode = 0;
            return false;
        }
        std::cout << "[DEBUG] HttpRequestParser: Handling chunked request" << std::endl;
        chunkReqHandler(req, bodyPart, clientMaxBodySize, errorCode);
        return errorCode == 0;
    }

    std::size_t contentLength = req.getContentLength();
    if (contentLength >= clientMaxBodySize) {
        std::cerr << "[ERROR] HttpRequestParser: Exceeded request max body size" << std::endl;
        errorCode = 413;
        return false;
    }

    if (bodyPart.size() < contentLength) {
        std::cout << "[INFO] HttpRequestParser: Incomplete request body, server reads again" << std::endl;
        errorCode = 0;
        return false;
    }

    std::string bodyContent = bodyPart.substr(0, contentLength);
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
        return false;
    }

    if (req.getVersion() != "HTTP/1.0" && req.getVersion() != "HTTP/1.1") {
        errorCode = 505; // HTTP Version Not Supported
        return false;
    }

    if (req.getPath()[0] != '/') {
        errorCode = 400;
        return false;
    }

    if (!transferEncoding.empty() && req.getContentLength() > 0) {
        errorCode = 400; // Bad Request
        std::cerr << "Conflicting Transfer-Encoding and Content-Length" << std::endl;
        return false;
    }

    // validate header keys and values

    return true;
}

bool isValidHeader(std::strin& key){
    switch (key) {
        case "CONTENT-TYPE": return true;
        case "CONTENT-ENCODING": return true;
        case "CONTENT-LANGUAGE": return true;
        case "CONTENT-LOCATION": return true;
        case "CONTENT-LENGTH": return true;
        case "CONTENT_RANGE": return true;
        case "TRAILER": return true;
        case "TRANSFER-ENCODING": return true;
        case "CACHE-CONTROL": return true;
        case "CONNECTION": return true;
        case "EXPECT": return true;
        case "HOST": return true;
        case "MAX-FORWARDS": return true;
        case "PRAGMA": return true;
        case "RANGE": return true;
        case "TE": return true;
        case "IF-MATCH": return true;
        case "IF-NONE-MATCH": return true;
        case "IF-MODIFIED-SINCE": return true;
        case "IF-UNMODIFIED-SINCE": return true;
        case "IF-RANGE": return true;
        case "ACCEPT": return true;
        case "ACCEPT-CHARSET": return true;
        case "ACCEPT-ENCODING": return true;
        case "ACCEPT-LANGUAGE": return true
        case "AUTHORIZATION": return true;
        case "PROXY-AUTHORIZATION": return true;
        case "FROM": return true;
        case "REFERER": return true;
        case "USER-AGENT": return true;
        case "AGE": return true;
        case "EXPIRES": return true;
        case "DATE": return true;
        case "LOCATION": return true;
        case "RETRY-AFTER": return true;
        case "VARY": return true;
        case "WARNING": return true;
        case "ETAG": return true;
        case "LAST-MODIFIED": return true;
        case "WWW-AUTHENTICATE": return true;
        case "PROXY-AUTHENTICATE": return true;
        case "ACCEPT-RANGES": return true;
        case "ALLOW": return true;
        case "SERVER": return true;
        case "MIME_VERSION": return true;
        default: return false;
    }
}
