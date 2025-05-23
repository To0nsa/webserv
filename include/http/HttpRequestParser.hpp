
#pragma once

#include "http/HttpRequest.hpp"
#include "utils/stringUtils.hpp"

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
