/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponseBuilder.cpp                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:14:23 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/28 21:12:45 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpResponseBuilder.hpp"
#include "http/HttpResponse.hpp"
#include "utils/filesystemUtils.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace MessageHandler {

std::string getDefaultMessage(int status_code) {
    // Define a local alias for better readability and maintainability
    using StatusMessageMap = std::map<int, std::string>;
    // Static map ensures it's initialized once and reused on every call
    static const StatusMessageMap status_messages = {{200, "OK"},
                                                     {201, "Created"},
                                                     {301, "Moved Permanently"},
                                                     {302, "Found"},
                                                     {400, "Bad Request"},
                                                     {403, "Forbidden"},
                                                     {404, "Not Found"},
                                                     {405, "Method Not Allowed"},
                                                     {408, "Request Timeout"},
                                                     {411, "Length Required"},
                                                     {413, "Payload Too Large"},
                                                     {414, "URI Too Long"},
                                                     {431, "Request Header Fields Too Large"},
                                                     {500, "Internal Server Error"},
                                                     {501, "Not Implemented"},
                                                     {502, "Bad Gateway"},
                                                     {503, "Service Unavailable"},
                                                     {505, "HTTP Version Not Supported"}};

    // Attempt to find the status code in the map
    StatusMessageMap::const_iterator it = status_messages.find(status_code);
    // Return the associated message or a fallback if unknown
    return (it != status_messages.end()) ? it->second : "Unknown Error";
}
} // namespace MessageHandler

namespace {

// Determines keep-alive status from HTTP version and connection header.
bool shouldKeepAlive(const std::string& version, const std::string& conn) {
    // HTTP/1.1 defaults to keep-alive unless explicitly closed
    if (version == "HTTP/1.1")
        return conn != "close";
    // HTTP/1.0 defaults to close unless explicitly kept alive
    if (version == "HTTP/1.0")
        return conn == "keep-alive";
    // Unknown or unsupported version → safest behavior: close
    return false;
}

// Common initialization for any response.
void initializeResponse(HttpResponse& response, int status_code, const std::string& message,
                        const HttpRequest& request) {
    // Set HTTP status code and message (e.g., 200 OK)
    response.setStatus(status_code, message);
    // Store request version and connection header to guide connection persistence
    response.setRequestMeta(request.getVersion(), request.getHeader("Connection"));

    // Extract relevant headers from the request for keep-alive logic
    const std::string& conn    = request.getHeader("Connection");
    const std::string& version = request.getVersion();

    // These status codes indicate the server should always close the connection
    static const std::set<int> force_close_codes = {400, 408, 413, 500};

    // Determine if we should keep the connection alive based on protocol rules and status
    bool keep_alive = shouldKeepAlive(version, conn) && !force_close_codes.count(status_code);

    // Set the appropriate Connection header in the response
    response.setHeader("Connection", keep_alive ? "keep-alive" : "close");
}

} // anonymous namespace

namespace ResponseBuilder {

HttpResponse generateSuccess(int status_code, const std::string& body,
                             const std::string& content_type, const HttpRequest& request) {
    HttpResponse response;
    // Get the default reason phrase for the given status code (e.g., "OK")
    std::string message = MessageHandler::getDefaultMessage(status_code);

    // Set status, connection headers, and keep-alive logic
    initializeResponse(response, status_code, message, request);
    // Set the content type (e.g., "text/html", "application/json")
    response.setHeader("Content-Type", content_type);
    // Attach the response body
    response.setBody(body);
    return response;
}

HttpResponse generateError(int status_code, const Server& server, const HttpRequest& request) {
    HttpResponse response;
    std::string  message = MessageHandler::getDefaultMessage(status_code);
    std::string  body;

    // Try custom error page
    const auto& error_pages = server.getErrorPages();
    auto        it          = error_pages.find(status_code);
    if (it != error_pages.end()) {
        std::string uri = it->second;
        // Strip leading slash
        if (!uri.empty() && uri.front() == '/')
            uri.erase(0, 1);
        // Determine default root (location "/")
        std::string default_root;
        for (const auto& loc : server.getLocations()) {
            if (loc.getPath() == "/") {
                default_root = loc.getRoot();
                break;
            }
        }
        std::string   full_path = joinPath(default_root, uri);
        std::ifstream file(full_path.c_str());
        if (file)
            body.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }

    // Fallback default page
    if (body.empty()) {
        std::ostringstream ss;
        ss << "<html><body><h1>" << status_code << " " << message << "</h1></body></html>";
        body = ss.str();
    }

    initializeResponse(response, status_code, message, request);
    response.setHeader("Content-Type", "text/html");
    response.setBody(body);
    return response;
}

HttpResponse generateRedirect(int status_code, const std::string& location,
                              const HttpRequest& request) {
    HttpResponse response;
    // Get default reason phrase for the redirect status code (e.g., "Moved Permanently")
    std::string message = MessageHandler::getDefaultMessage(status_code);

    // Set status line, connection handling, and keep-alive logic
    initializeResponse(response, status_code, message, request);
    // Set the Location header to indicate the redirect target
    response.setHeader("Location", location);
    // No body is sent in most redirects → explicitly set Content-Length to 0
    // response.setHeader("Content-Length", "0");
    return response;
}

} // namespace ResponseBuilder
