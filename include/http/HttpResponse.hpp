/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 10:55:37 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/11 13:10:21 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# pragma once

#include <string>
#include <map>

class HttpResponse {
private:
	int _status_code;
	std::string _status_message;
	std::map<std::string, std::string> _headers;
	std::string _body;

public:
	HttpResponse( void );
	~HttpResponse( void );
	HttpResponse( const HttpResponse& other ) = default;
	HttpResponse& operator=( const HttpResponse& other ) = default;

	void setStatus( int code, const std::string& message );
	void setHeader( const std::string& key, const std::string& value );
	void setBody( const std::string& body );

	std::string toString( void ) const;
};
