/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:37:06 by irychkov          #+#    #+#             */
/*   Updated: 2025/04/30 11:01:37 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <string>
#include <vector>
#include <map>
#include "Location.hpp"

class Server {

	private:
		int _port;
		std::string _host;
		std::vector<std::string> _server_names;
		std::map<int, std::string> _error_pages;
		size_t _client_max_body_size;
		std::vector<Location> _locations;
	
	public:
		Server( void );
		~Server( void );
		Server( const Server& other ) = default;
		Server& operator=( const Server& other ) = default;

		// Setters
		void setPort( int port );
		void setHost( const std::string& host );
		void addServerName( const std::string& name );
		void setErrorPage( int code, const std::string& path );
		void setClientMaxBodySize( size_t size );
		void addLocation( const Location& location );

		// Getters
		int getPort( void ) const;
		const std::string& getHost( void ) const;
		const std::vector<std::string>& getServerNames( void ) const;
		const std::map<int, std::string>& getErrorPages( void ) const;
		size_t getClientMaxBodySize( void ) const;
		const std::vector<Location>& getLocations( void ) const;
	};
