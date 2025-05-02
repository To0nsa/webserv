/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/01 20:23:05 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/01 20:50:11 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpResponse.hpp"

std::string buildResponse(int statusCode, const std::string& body, const std::string& contentType) {
	std::stringstream response;
	std::string statusText;

	switch (statusCode) {
		case 200: statusText = "OK"; break;
		case 201: statusText = "Created"; break;
		case 204: statusText = "No Content"; break;
		case 301: statusText = "Moved Permanently"; break;
		case 302: statusText = "Found"; break;
		case 400: statusText = "Bad Request"; break;
		case 403: statusText = "Forbidden"; break;
		case 404: statusText = "Not Found"; break;
		case 405: statusText = "Method Not Allowed"; break;
		case 413: statusText = "Payload Too Large"; break;
		case 500: statusText = "Internal Server Error"; break;
		default:  statusText = "Error"; break;
	}

	response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
	response << "Content-Type: " << contentType << "\r\n";
	response << "Content-Length: " << body.size() << "\r\n";
	response << "Connection: close\r\n\r\n";
	response << body;

	return response.str();
}

std::string buildErrorBody(const Server& server, int code) {
	std::map<int, std::string> pages = server.getErrorPages();
	std::map<int, std::string>::const_iterator it = pages.find(code);

	if (it != pages.end()) {
		HttpRequest fake;
		std::string uri = it->second;
		fake.parse("GET " + uri + " HTTP/1.1\r\nHost: x\r\n\r\n");
		std::string errorPath = buildFilePath(server, fake);
		std::ifstream file(errorPath.c_str());
		if (file) {
			std::stringstream ss;
			ss << file.rdbuf();
			return ss.str();
		}
	}

	std::stringstream fallback;
	fallback << "<h1>" << code << " Error</h1>";
	return fallback.str();
}
