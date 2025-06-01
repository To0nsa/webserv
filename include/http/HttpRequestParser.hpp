
#pragma once

#include "http/HttpRequest.hpp"
#include "utils/stringUtils.hpp"
#include <string>
#include "utils/Logger.hpp"
#include "utils/filesystemUtils.hpp"
#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <regex>
#include <set>
#include <sstream>

namespace HttpRequestParserUtils {
    bool isValidHttpMethodToken(const std::string& method);
    bool isValidPath(const std::string& rawPath);
    Url parseUrl(HttpRequest& req, const std::string& url);
    bool insertValidatedHeader(HttpRequest& req, const std::string& key, const std::string& value, int& errorCode);
    bool isChunkedBodyComplete(const std::string& bodyPart);
    void chunkReqHandler(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                     int& errorCode, std::size_t& consumedBytes);
}


class HttpRequestParser {
  public:
    static bool parse(HttpRequest& req, const std::string& raw_req, std::size_t clientMaxBodySize,
                      int& errorCode, std::size_t& consumedBytes);

  private:
    HttpRequestParser()                                          = delete;
    ~HttpRequestParser()                                         = delete;
    HttpRequestParser(const HttpRequestParser& org)              = delete;
    HttpRequestParser& operator=(const HttpRequestParser& other) = delete;
};
