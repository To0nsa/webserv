/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponseBuilder.cpp                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:14:23 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/12 14:07:10 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpResponseBuilder.hpp"
#include "http/HttpResponse.hpp"
#include <fstream>
#include <map>
#include <sstream>
#include <iostream>

namespace MessageHandler {

std::string getDefaultMessage(int status_code) {
        static const std::map<int, std::string> status_messages = {
            {200, "OK"},
            {201, "Created"},
            {204, "No Content"},
            {301, "Moved Permanently"},
            {302, "Found"},
            {400, "Bad Request"},
            {403, "Forbidden"},
            {404, "Not Found"},
            {405, "Method Not Allowed"},
            {408, "Request Timeout"},
            {413, "Payload Too Large"},
            {500, "Internal Server Error"},
            {502, "Bad Gateway"},
            {503, "Service Unavailable"}
        };

    std::map<int, std::string>::const_iterator it = status_messages.find(status_code);
    if (it != status_messages.end())
        return it->second;
    else
        return "Unknown Error";
}
} // namespace MessageHandler

namespace ResponseBuilder {

    HttpResponse generateSuccess(int status_code, const std::string& body,
                                const std::string& content_type, const HttpRequest& request) {
        HttpResponse response;
        std::string  message = MessageHandler::getDefaultMessage(status_code);

        response.setStatus(status_code, message);
        response.setHeader("Content-Type", content_type);
        response.setBody(body);

        std::string conn = request.getHeader("Connection");
        std::string meth = request.getMethod();
        std::string version = request.getVersion();
        bool keep_alive = false;

        if (version == "HTTP/1.1")
            keep_alive = (conn != "close");
        else if (version == "HTTP/1.0")
            keep_alive = (conn == "keep-alive");

        if (keep_alive)
            response.setHeader("Connection", "keep-alive");
        else
            response.setHeader("Connection", "close");

        return response;
    }

    HttpResponse generateError(int status_code, const Server& server, const HttpRequest& request) {
        HttpResponse response;
        std::string  message = MessageHandler::getDefaultMessage(status_code);
        std::string  body;

        const std::map<int, std::string>& error_pages = server.getErrorPages();
        std::map<int, std::string>::const_iterator it = error_pages.find(status_code);

        if (it != error_pages.end()) {
            std::ifstream file(it->second.c_str());
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                body = buffer.str();
            }
        }

        if (body.empty()) {
            std::stringstream ss;
            ss << "<html><body><h1>" << status_code << " " << message << "</h1></body></html>";
            body = ss.str();
        }

        response.setStatus(status_code, message);
        response.setHeader("Content-Type", "text/html");

        std::string conn = request.getHeader("Connection");
        std::string version = request.getVersion();
        bool client_wants_keep_alive = (conn == "keep-alive");
        static const std::set<int> force_close_codes = {400, 408, 413, 500};
        bool force_close = force_close_codes.count(status_code);

        if (force_close || version == "HTTP/1.0" || !client_wants_keep_alive)
            response.setHeader("Connection", "close");
        else
            response.setHeader("Connection", "keep-alive");
        return response;
    }

    HttpResponse generateRedirect(int status_code, const std::string& location,
                                const HttpRequest& request) {
        HttpResponse response;
        std::string  message = MessageHandler::getDefaultMessage(status_code);

        response.setStatus(status_code, message);
        response.setHeader("Location", location);
        response.setHeader("Content-Length", "0");
        std::string conn = request.getHeader("Connection");
        std::string version = request.getVersion();
        bool keep_alive = false;

        if (version == "HTTP/1.1")
            keep_alive = (conn != "close");
        else if (version == "HTTP/1.0")
            keep_alive = (conn == "keep-alive");
        return response;
    }
} // namespace ResponseBuilder
