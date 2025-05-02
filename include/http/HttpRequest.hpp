/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/01 11:25:47 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/01 11:49:26 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <string>
#include <map>

class HttpRequest {

	private:
		std::string _method;
		std::string _path;
		std::string _version;
		std::map<std::string, std::string> _headers;
		std::string _body;

	public:
		HttpRequest( void );
		~HttpRequest( void );

		bool parse( const std::string& raw_request );
		void printRequest( void ) const;

		const std::string& getMethod( void ) const;
		const std::string& getPath( void ) const;
		const std::string& getVersion( void ) const;
		const std::string& getHeader( const std::string& key ) const;
		const std::string& getBody( void ) const;
};
