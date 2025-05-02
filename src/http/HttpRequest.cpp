/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/01 11:27:51 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/01 11:52:31 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpRequest.hpp"
#include <sstream>
#include <algorithm>
#include <iostream>

HttpRequest::HttpRequest( void ) { }

HttpRequest::~HttpRequest( void ) { }

void HttpRequest::printRequest() const {
	std::cout << "===== Incoming HTTP Request =====" << std::endl;
	std::cout << _method << " " << _path << " " << _version << std::endl;

	std::cout << "----- Headers -----" << std::endl;
	for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
		std::cout << it->first << ": " << it->second << std::endl;
	}

	if (!_body.empty()) {
		std::cout << "----- Body -----" << std::endl;
		std::cout << _body << std::endl;
	}

	std::cout << "===============================" << std::endl;
}

bool HttpRequest::parse( const std::string& raw_request ) {
	std::istringstream stream(raw_request);
	std::string line;

	// Parse request line
	if (!std::getline(stream, line) || line.empty())
		return (false);

	size_t method_end = line.find(' ');
	size_t path_end = line.find(' ', method_end + 1);
	if (method_end == std::string::npos || path_end == std::string::npos)
		return (false);

	_method = line.substr(0, method_end);
	_path = line.substr(method_end + 1, path_end - method_end - 1);
	_version = line.substr(path_end + 1);

	// Parse headers
	while (std::getline(stream, line) && !line.empty() && line != "\r") {
		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;

		std::string key = line.substr(0, colon);
		std::string value = line.substr(colon + 1);

		// Trim leading/trailing whitespace
		key.erase(key.find_last_not_of(" \t\r\n") + 1);
		value.erase(0, value.find_first_not_of(" \t"));
		value.erase(value.find_last_not_of(" \t\r\n") + 1);

		_headers[key] = value;
	}

	// Extract body (if any)
	std::string body;
	while (std::getline(stream, line)) {
		_body += line + "\n";
	}
	if (!_body.empty() && _body[_body.size() - 1] == '\n')
		_body.erase(_body.size() - 1);

	return (true);
}

const std::string& HttpRequest::getMethod( void ) const {
	return (_method);
}

const std::string& HttpRequest::getPath( void ) const {
	return (_path);
}

const std::string& HttpRequest::getVersion( void ) const {
	return (_version);
}

const std::string& HttpRequest::getHeader(const std::string& key) const {
	static const std::string empty = "";
	std::map<std::string, std::string>::const_iterator it = _headers.find(key);
	if (it != _headers.end())
		return (it->second);
	return (empty);
}

const std::string& HttpRequest::getBody( void ) const {
	return (_body);
}
