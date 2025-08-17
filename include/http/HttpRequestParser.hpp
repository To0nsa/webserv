
#pragma once

#include <cstddef> // for size_t
#include <string>  // for string
#include <vector>  // for vector
class HttpRequest;
class Server;

class HttpRequestParser {
  public:
    static bool parse(HttpRequest& req, const std::string& raw_req,
                      std::vector<Server> serversOnPort, int& errorCode,
                      std::size_t& consumedBytes);

  private:
    HttpRequestParser()                                          = delete;
    ~HttpRequestParser()                                         = delete;
    HttpRequestParser(const HttpRequestParser& org)              = delete;
    HttpRequestParser& operator=(const HttpRequestParser& other) = delete;
};
