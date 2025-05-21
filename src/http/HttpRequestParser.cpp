
#include "http/HttpRequestParser.hpp"
#include <sstream>
#include <algorithm>
#include <regex>
#include <ranges>
#include <iostream>

//----------------------------------------
// CLASS METHODS
//----------------------------------------

bool parseReqHeader(HttpRequest &req, const std::string& headerPart, int& errorCode);
bool parseReqBody(HttpRequest&req, const std::string& bodyPart, std::size_t clientMaxBodySize, int& errorCode);
bool isChunkedBodyComplete(const std::string& bodyPart);
Url parseUrl(const std::string& url);


bool HttpRequestParser::parse(HttpRequest &req, const std::string& raw_req, std::size_t clientMaxBodySize, int& errorCode)
{
    std::size_t headerEndPos = raw_req.find("\r\n\r\n");
    if (headerEndPos == std::string::npos) {
        errorCode = 0; // Incomplete request
        std::cout << "Incomplete header, we read again" << std::endl;
        return false;
    }
    std::string headerPart = raw_req.substr(0, headerEndPos);
    std::string bodyPart = raw_req.substr(headerEndPos + 4);

    // std::cout << "HPART: " << headerPart << std::endl;
    // std::cout << "BPART: " << bodyPart << std::endl;

    if (!parseReqHeader(req, headerPart, errorCode))
        return false;
    if (!parseReqBody(req, bodyPart, clientMaxBodySize, errorCode))
        return false;
    return true;
}

bool parseReqHeader(HttpRequest &req, const std::string& headerPart, int& errorCode)
{
    std::istringstream stream(headerPart);
    std::string line;
    if (!std::getline(stream, line) || line.empty()) {
        errorCode = 400;
        return false;
    }
    std::istringstream requestLineStream(line);
    std::string method, path, version;
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
        std::string key = toUpper(line.substr(0, colonPos));
        std::string value = line.substr(colonPos + 1);
        key.erase(key.find_last_not_of(" \t\r\n") + 1); // remove trailing whitespace
        value.erase(0, value.find_first_not_of(" \t\r\n")); // remove leading whitespace

        if ((key == "TRANSFER-ENCODING") && value == "chunked" && req.getMethod() == "GET") {
            std::cerr << "Chunked transfer encoding is not allowed in GET requests" << std::endl;
            errorCode = 400;
            return false;
        }

        if (key == "CONTENT-LENGTH") {
            if (value.empty() || !std::all_of(value.begin(), value.end(),
    [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) {
                std::cerr << "Invalid Content-Length value" << std::endl;
                errorCode = 411;
                return false;
            }

            try {
                req.setContentLength(std::stoull(value));

            } catch(const std::exception& e) {
                std::cerr << "Invalid Content-Length value: " << e.what() << std::endl;
                errorCode = 411;
                return false;
            }
        }
        req.setHeader(key, value);
    }
    if (req.getHeader("HOST").empty()) {
        std::cerr << "Missing HOST header" << std::endl;
        errorCode = 400;
        return false;
    }
    try {
        Url url = parseUrl(req.getHeader("HOST") + req.getPath());
        req.setUrl(url);
    } catch (...) {
        errorCode = 400;
        return false;
    }


    return true;
}

bool isChunkedBodyComplete(const std::string& bodyPart) {
    return bodyPart.find("\r\n0\r\n\r\n") != std::string::npos;
}

void chunkReqHandler(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize, int& errorCode) {
    std::istringstream stream(bodyPart);
    std::string chunkLine;
    std::string fullBody;
    std::size_t totalSize = 0;

    while (std::getline(stream, chunkLine)) {
        if (!chunkLine.empty() && chunkLine.back() == '\r') {
            chunkLine.pop_back();
        }

        std::size_t chunkSize = 0;
        try {
            chunkSize = std::stoul(chunkLine, nullptr, 16);  // Hexadecimal
        } catch (...) {
            errorCode = 400;
            std::cerr << "Invalid chunk size format" << std::endl;
            return;
        }

        if (chunkSize == 0) {
            break; // End of chunks
        }

        if (totalSize + chunkSize > clientMaxBodySize) {
            std::cerr << "Exceeded request max body size in chunked transfer" << std::endl;
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



bool parseReqBody(HttpRequest&req, const std::string& bodyPart, std::size_t clientMaxBodySize, int& errorCode) {
    if (req.getMethod() == "GET") {
        errorCode = 0;
        return true;
    }
    const std::string& transferEncoding = req.getHeader("TRANSFER-ENCODING");

    if (transferEncoding == "chunked") {
        if (!isChunkedBodyComplete(bodyPart)) {
            std::cout << "Incomplete chunked body, we read again" << std::endl;
            errorCode = 0;
            return false;
        }
        chunkReqHandler(req, bodyPart, clientMaxBodySize, errorCode);
        return errorCode == 0;
    }

    std::size_t contentLength = req.getContentLength();
    if (contentLength >= clientMaxBodySize) {
        std::cerr << "Exceeded request max body size" << std::endl;
        errorCode = 413;
        return false;
    }

    if (bodyPart.size() < contentLength) {
        std::cerr << "Incomplete request body, we read again" << std::endl;
        errorCode = 0;
        return false;
    }

    std::string bodyContent = bodyPart.substr(0, contentLength);
    req.setBody(bodyContent);
    errorCode = 0;
    return true;
}

Url parseUrl(const std::string& url) {
    Url res;
    std::regex urlRegex(R"((https?://)?(?:([^:@]+)(?::([^:@]*))?@)?([^:/?#]+)(?::(\d+))?(/[^?#]*)?(?:\?([^#]*))?(?:#(.*))?)");
    std::smatch matches;

    if (!std::regex_match(url, matches, urlRegex)) {
        throw std::invalid_argument("Invalid URL");
    }

    res.scheme = matches[1].str();
    res.user = matches[2].str();
    res.password = matches[3].str();
    res.host = matches[4].str();
    res.port = matches[5].str();
    res.path = matches[6].str();
    res.query = matches[7].str();
    res.fragment = matches[8].str();
    return res;
}
