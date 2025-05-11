/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponseBuilder.cpp                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:14:23 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/11 14:45:50 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpResponseBuilder.hpp"
#include "http/HttpResponse.hpp"
#include <map>
#include <sstream>
#include <fstream>

namespace MessageHandler {

	std::string getDefaultMessage(int status_code) {
		std::map<int, std::string> status_messages = {
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

		if (status_messages.count(status_code))
			return status_messages[status_code];
		else
			return "Unknown Error";
	}
}

namespace ResponseBuilder {

	HttpResponse generateSuccess( int status_code, const std::string& body, const std::string& content_type, const HttpRequest& request) {
		HttpResponse response;
		std::string message = MessageHandler::getDefaultMessage(status_code);

		response.setStatus(status_code, message);
		response.setHeader("Content-Type", content_type);
		response.setBody(body);

		std::string conn = request.getHeader("Connection");
		bool keep_connection = (conn == "keep-alive");

		if (!keep_connection)
			response.setHeader("Connection", "close");
		else
			response.setHeader("Connection", "keep-alive");

		return response;
	}
	

	HttpResponse generateError(int status_code, const Server& server, const HttpRequest& request) {
		HttpResponse response;
		std::string message = MessageHandler::getDefaultMessage(status_code);
		std::string body;

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
		bool keep_connection = (conn == "keep-alive");

		if (!keep_connection)
			response.setHeader("Connection", "close");
		else
			response.setHeader("Connection", "keep-alive");

		return response;
	}

	HttpResponse generateRedirect(int status_code, const std::string& location, const HttpRequest& request) {
		HttpResponse response;
		std::string message = MessageHandler::getDefaultMessage(status_code);
	
		response.setStatus(status_code, message);
		response.setHeader("Location", location);
		response.setHeader("Content-Length", "0");
		std::string conn = request.getHeader("Connection");
		bool close_connection = (conn == "close");

		if (close_connection)
			response.setHeader("Connection", "close");
		else
			response.setHeader("Connection", "keep-alive");

		return response;
	}
}
