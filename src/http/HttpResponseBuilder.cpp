/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponseBuilder.cpp                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:14:23 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/25 13:32:53 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpResponseBuilder.hpp"
#include "http/HttpResponse.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace MessageHandler {

std::string getDefaultMessage(int status_code) {
    using StatusMessageMap                        = std::map<int, std::string>;
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
                                                     {431, "Request Header Fields Too Large"},
                                                     {500, "Internal Server Error"},
                                                     {501, "Not Implemented"},
                                                     {502, "Bad Gateway"},
                                                     {503, "Service Unavailable"}};
    auto                          it              = status_messages.find(status_code);
    return (it != status_messages.end()) ? it->second : "Unknown Error";
}

} // namespace MessageHandler

namespace {

// Determine keep-alive semantics
bool shouldKeepAlive(const std::string& version, const std::string& conn) {
    if (version == "HTTP/1.1")
        return conn != "close";
    if (version == "HTTP/1.0")
        return conn == "keep-alive";
    return false;
}

// Common init: status, connection header, keep-alive logic
void initializeResponse(HttpResponse& response, int status_code, const std::string& message,
                        const HttpRequest& request) {
    response.setStatus(status_code, message);
    response.setRequestMeta(request.getVersion(), request.getHeader("Connection"));

    const std::set<int> force_close_codes = {400, 408, 413, 500};
    bool keep_alive = shouldKeepAlive(request.getVersion(), request.getHeader("Connection")) &&
                      !force_close_codes.count(status_code);
    response.setHeader("Connection", keep_alive ? "keep-alive" : "close");
}

} // anonymous namespace

namespace ResponseBuilder {

HttpResponse generateSuccess(int status_code, const std::string& body,
                             const std::string& content_type, const HttpRequest& request) {
    HttpResponse response;
    std::string  message = MessageHandler::getDefaultMessage(status_code);

    // Status+Connection
    initializeResponse(response, status_code, message, request);
    // Type + Body
    response.setHeader("Content-Type", content_type);
    response.setBody(body);
    // Ensure non-chunked response: set Content-Length
    response.setHeader("Content-Length", std::to_string(body.size()));
    return response;
}

HttpResponse generateError(int status_code, const Server& server, const HttpRequest& request) {
    HttpResponse response;
    std::string  message = MessageHandler::getDefaultMessage(status_code);
    std::string  body;

    const auto& error_pages = server.getErrorPages();
    auto        it          = error_pages.find(status_code);
    if (it != error_pages.end()) {
        std::ifstream file(it->second);
        if (file)
            body.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }
    if (body.empty()) {
        std::ostringstream ss;
        ss << "<html><body><h1>" << status_code << " " << message << "</h1></body></html>";
        body = ss.str();
    }

    initializeResponse(response, status_code, message, request);
    response.setHeader("Content-Type", "text/html");
    response.setBody(body);
    response.setHeader("Content-Length", std::to_string(body.size()));
    return response;
}

HttpResponse generateRedirect(int status_code, const std::string& location,
                              const HttpRequest& request) {
    HttpResponse response;
    std::string  message = MessageHandler::getDefaultMessage(status_code);

    initializeResponse(response, status_code, message, request);
    response.setHeader("Location", location);
    // No body, explicitly set zero length
    response.setHeader("Content-Length", "0");

    return response;
}

} // namespace ResponseBuilder
