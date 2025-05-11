/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 10:56:54 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/11 11:10:44 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpResponse.hpp"
#include <sstream>

HttpResponse :: HttpResponse( void ) {
	_status_code = 200;
	_status_message = "OK";
}

HttpResponse :: ~HttpResponse( void ) {}

void HttpResponse :: setStatus(int code, const std::string& message) {
	_status_code = code;
	_status_message = message;
}

void HttpResponse :: setHeader(const std::string& key, const std::string& value) {
	_headers[key] = value;
}

void HttpResponse :: setBody(const std::string& body) {
	_body = body;
}

std::string HttpResponse :: toString( void ) const {
	std::stringstream ss;

	ss << "HTTP/1.1 " << _status_code << " " << _status_message << "\r\n";

	for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it)
		ss << it->first << ": " << it->second << "\r\n";

	ss << "Content-Length: " << _body.length() << "\r\n";
	ss << "\r\n";
	ss << _body;

	return ss.str();
}
