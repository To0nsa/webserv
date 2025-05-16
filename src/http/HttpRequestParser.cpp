
#include "http/HttpRequestParser.hpp"
#include <sstream>
#include <algorithm>

//----------------------------------------
// CLASS METHODS
//----------------------------------------

bool parseReqHeader(HttpRequest &req, const std::string& headerPart);

bool HttpRequestParser::parse(HttpRequest &req, const std::string& raw_req)
{
  std::size_t headerEndPos = raw_req.find("\r\n\r\n");
  if (headerEndPos == std::string::npos) {
      return false;
  }
  std::string headerPart = raw_req.substr(0, headerEndPos);
  std::string bodyPart = raw_req.substr(headerEndPos + 4);

  parseReqHeader(req, headerPart);
  parseReqBody(req, bodyPart);

}


//----------------------------------------
// UTILITY FUNCTIONS
//----------------------------------------

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
      std::string key = line.substr(0, colonPos);
      std::string value = line.substr(colonPos + 1);
      key.erase(key.find_last_not_of(" \t\r\n") + 1); // remove trailing whitespace
      value.erase(0, value.find_first_not_of(" \t\r\n")); // remove leading whitespace

      if ((key == "TRANSFER_ENCODING") && value == "chunked" && req.getMethod() == "GET") {
          throw std::invalid_argument("Chunked transfer encoding is not allowed in GET requests");
      }

      if (key == "CONTENT_LENGTH") {
          if (value.empty() || !std::ranges::all_of(value, ::isdigit)) {
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
  return true;
}

bool chunkReqHandlder(HttpRequest&req, const std::string& bodyPart) {

}


bool parseReqBody(HttpRequest&req, const std::string& bodyPart) {

  const std::string& transferEncoding = req.getHeader("TRANSFER_ENCODING");
  if (transferEncoding == "chunked") {

  }
  return true;
}
