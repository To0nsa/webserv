/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:51:19 by irychkov          #+#    #+#             */
/*   Updated: 2025/04/30 10:01:17 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

Server :: Server() {
	_port = 80;							// Default HTTP port
	_host = "0.0.0.0";					// Default to all interface
	_client_max_body_size = 1048576;	// 1 MB
}

Server :: ~Server() { }

void Server :: setPort( int port ) {
	_port = port;
}

void Server :: setHost( const std::string& host ) {
	_host = host;
}

void Server :: addServerName( const std::string& name ) {
	_server_names.push_back( name );
}

void Server :: setErrorPage( int code, const std::string& path ) {
	_error_pages[code] = path;
}

void Server :: setClientMaxBodySize( size_t size ) {
	_client_max_body_size = size;
}

void Server :: addLocation( const Location& location ) {
	_locations.push_back( location );
}

int Server :: getPort( void ) const {
	return _port;
}

const std::string& Server :: getHost( void ) const {
	return _host;
}

const std::vector<std::string>& Server :: getServerNames( void ) const {
	return _server_names;
}

const std::map<int, std::string>& Server :: getErrorPages( void ) const {
	return _error_pages;
}

size_t Server :: getClientMaxBodySize( void ) const {
	return _client_max_body_size;
}

const std::vector<Location>& Server :: getLocations( void ) const {
	return _locations;
}
