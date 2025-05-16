
#pragma once

#include "http/HttpRequest.hpp"

class HttpRequestParser
{
    static bool parse(HttpRequest &req, const std::string& raw_req);
    private:
        HttpRequestParser() = delete;
        ~HttpRequestParser() = delete;
        HttpRequestParser(const HttpRequestParser& org) = delete;
        HttpRequestParser& operator=(const HttpRequestParser &other) = delete;
};
