/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 10:08:30 by irychkov          #+#    #+#             */
/*   Updated: 2025/04/30 13:52:22 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <iostream>
#include <fstream>
#include "Location.hpp"
#include "Server.hpp"

class ConfigParser {

	public:
		ConfigParser( void );
		~ConfigParser( void );
		ConfigParser( const ConfigParser& other ) = default;
		ConfigParser& operator=( const ConfigParser& other ) = default;
		std::vector<Server> parseConfig( const std::string& filepath );
		class CustomError : public std::exception
		{
			private:
				std::string _msg;
			public:
				explicit CustomError(const std::string& msg);
				virtual const char* what() const throw();
		};

	private:
		void parseServer( std::ifstream& file, Server& server );
		void parseLocation( std::ifstream& file, Location& location, int& line_number );
	};
