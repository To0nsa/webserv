
#include "http/HttpRequestParser.hpp"
#include <sstream>
#include <algorithm>
#include <regex>
#include <ranges>
#include <iostream>

//----------------------------------------
// CLASS METHODS
//----------------------------------------

bool parseReqHeader(HttpRequest &req, const std::string& headerPart);
void parseReqBody(HttpRequest&req, const std::string& bodyPart, std::size_t clientMaxBodySize);
Url parseUrl(const std::string& url);


bool HttpRequestParser::parse(HttpRequest &req, const std::string& raw_req, std::size_t clientMaxBodySize)
{
    std::size_t headerEndPos = raw_req.find("\r\n\r\n");
    if (headerEndPos == std::string::npos) {
        return false;
    }
    std::string headerPart = raw_req.substr(0, headerEndPos);
    std::string bodyPart = raw_req.substr(headerEndPos + 4);

    // std::cout << "HPART: " << headerPart << std::endl;
    // std::cout << "BPART: " << bodyPart << std::endl;

    parseReqHeader(req, headerPart);
    parseReqBody(req, bodyPart, clientMaxBodySize);
    return true;
}

bool parseReqHeader(HttpRequest &req, const std::string& headerPart)
{
    std::istringstream stream(headerPart);
    std::string line;
    if (!std::getline(stream, line) || line.empty()) {
        return false;
    }
    std::istringstream requestLineStream(line);
    std::string method, path, version;
    requestLineStream >> method >> path >> version;

    if (method.empty() || path.empty() || version.empty()) {
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
            throw std::invalid_argument("Chunked transfer encoding is not allowed in GET requests");
        }

        if (key == "CONTENT-LENGTH") {
            if (value.empty() || !std::ranges::all_of(value, [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) {
                throw std::invalid_argument("Invalid Content-Length: " + value);
            }

            try {
                req.setContentLength(std::stoull(value));

            } catch(const std::exception& e) {
                throw std::invalid_argument("Invalid Content-Length: " + value);
            }
        }
        req.setHeader(key, value);
    }
    if (req.getHeader("HOST").empty()) {
        throw std::invalid_argument("No Host found in header request");
    }
    Url url = parseUrl(req.getHeader("HOST") + req.getPath());
    req.setUrl(url);


    return true;
}

void chunkReqHandlder(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize) {
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
            throw std::invalid_argument("Invalid chunk size format");
        }

        if (chunkSize == 0) {
            break; // End of chunks
        }

        if (totalSize + chunkSize > clientMaxBodySize) {
            throw std::invalid_argument("Exceeded request max body size in chunked transfer");
        }

        std::string chunkData(chunkSize, '\0');
        stream.read(&chunkData[0], chunkSize);

        fullBody += chunkData;
        totalSize += chunkSize;

        // Consume trailing "\r\n" after the chunk
        std::getline(stream, chunkLine);
    }

    req.setBody(fullBody);
}



void parseReqBody(HttpRequest&req, const std::string& bodyPart, std::size_t clientMaxBodySize) {
    if (req.getMethod() == "GET") {
        return;
    }
    const std::string& transferEncoding = req.getHeader("TRANSFERENCODING");

    if (transferEncoding == "chunked") {
        return chunkReqHandlder(req, bodyPart, clientMaxBodySize);
    }

    std::size_t contentLength = req.getContentLength();
    if (contentLength >= clientMaxBodySize) {
        throw std::invalid_argument("Exceeded request max body size");
    }

    if (bodyPart.size() < contentLength) {
        return;
    }

    std::string bodyContent = bodyPart.substr(0, contentLength);
    req.setBody(bodyContent);
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
