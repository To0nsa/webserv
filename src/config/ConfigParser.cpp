/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 10:20:59 by irychkov          #+#    #+#             */
/*   Updated: 2025/04/30 14:52:21 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigParser.hpp"
#include <sstream>

ConfigParser :: ConfigParser( void ) { }

ConfigParser :: ~ConfigParser( void ) { }

// --- CustomError ---
ConfigParser::CustomError::CustomError( const std::string& msg ) {
	_msg = msg;
}

const char* ConfigParser::CustomError::what() const throw() {
	return (_msg.c_str());
}

/*
Helper function to trim whitespace from the beginning and end of a string
*/
static std::string trim( const std::string& s )
{
	size_t start = s.find_first_not_of(" \t\r\n");
	size_t end = s.find_last_not_of(" \t\r\n");
	if (start == std::string::npos)
		return ("");
	return (s.substr(start, end - start + 1));
}

/*
Helper function to remove comments from a string
*/
static std::string removeComment( const std::string& s )
{
	size_t pos = s.find('#');
	if (pos == std::string::npos)
		return (trim(s));
	else
		return (trim(s.substr(0, pos)));
}

void ConfigParser::parseLocation(std::ifstream& file, Location& location, int& line_number) {
	std::string line;
	int brace_depth = 1;

	while (std::getline(file, line)) {
		line_number++;
		line = removeComment(line);
		if (line.empty())
			continue;

		line = trim(line);
		if (line == "}") {
			brace_depth--;
			if (brace_depth == 0)
				break;
			continue;
		}

		if (line.compare(0, 4, "root") == 0) {
			std::string value = trim(line.substr(4));
			if (value.back() != ';')
				throw CustomError("Line " + std::to_string(line_number) + ": missing ';' in root");
			value.pop_back();
			location.root = trim(value);
		}
		else if (line.compare(0, 5, "index") == 0) {
			std::string value = trim(line.substr(5));
			if (value.back() != ';')
				throw CustomError("Line " + std::to_string(line_number) + ": missing ';' in index");
			value.pop_back();
			location.index = trim(value);
		}
		else if (line.compare(0, 7, "methods") == 0) {
			std::string values = trim(line.substr(7));
			if (values.back() != ';')
				throw CustomError("Line " + std::to_string(line_number) + ": missing ';' in methods");
			values.pop_back();
			std::stringstream ss(values);
			std::string method;
			while (ss >> method) {
				if (method != "GET" && method != "POST" && method != "DELETE")
					throw CustomError("Line " + std::to_string(line_number) + ": invalid HTTP method: " + method);
				location.methods.push_back(method);
			}
		}
		else if (line.compare(0, 13, "cgi_extension") == 0) {
			std::string value = trim(line.substr(13));
			if (value.back() != ';') throw CustomError("Line " + std::to_string(line_number) + ": missing ';' in cgi_pass");
			value.pop_back();
			location.cgi_extension = trim(value);
		}
		else if (line.compare(0, 6, "return") == 0) {
			std::string rest = trim(line.substr(6));
			if (rest.back() != ';')
				throw CustomError("Line " + std::to_string(line_number) + ": missing ';' in return");
			rest.pop_back();
		
			std::stringstream ss(rest);
			std::string first, second;
			ss >> first >> second;
		
			if (second.empty()) {
				location.return_code = 302;
				location.redirect = first;
			} else {
				location.return_code = std::stoi(first);
				location.redirect = second;
			}
		}
		else if (line.compare(0, 12, "upload_store") == 0) {
			std::string value = trim(line.substr(12));
			if (value.back() != ';')
				throw CustomError("Line " + std::to_string(line_number) + ": missing ';' in upload_store");
			value.pop_back();
			location.upload_store = trim(value);
		}
		else if (line.compare(0, 9, "autoindex") == 0) {
			std::string val = trim(line.substr(9));
			if (val.back() != ';')
				throw CustomError("Line " + std::to_string(line_number) + ": missing ';' in autoindex");
			val.pop_back();
			val = trim(val);
			if (val == "on")
				location.autoindex = true;
			else if (val == "off")
				location.autoindex = false;
			else
				throw CustomError("Line " + std::to_string(line_number) + ": autoindex must be 'on' or 'off'");
		}
		else {
			throw CustomError("Line " + std::to_string(line_number) + ": Unknown directive in location block: " + line);
		}
	}
}

void ConfigParser::parseServer(std::ifstream& file, Server& server) {
	std::string line;
	int line_number = 0;
	int brace_depth = 1;

	while (std::getline(file, line)) {
		line_number++;
		line = removeComment(line);
		if (line.empty())
			continue;

		line = trim(line);
		if (line == "}") {
			brace_depth--;
			if (brace_depth == 0)
				break;
			continue;
		}

		if (line.compare(0, 6, "listen") == 0) {
			std::string value = trim(line.substr(6));
			if (value.back() != ';')
				throw CustomError("Line " + std::to_string(line_number) + ": missing ';' in listen");
			value.pop_back();
			server.setPort(std::stoi(trim(value)));
		}
		else if (line.compare(0, 4, "host") == 0) {
			std::string value = trim(line.substr(4));
			if (value.back() != ';')
				throw CustomError("Line " + std::to_string(line_number) + ": missing ';' in host");
			value.pop_back();
			server.setHost(trim(value));
		}
		else if (line.compare(0, 11, "server_name") == 0) {
			std::string names = trim(line.substr(11));
			if (names.back() != ';')
				throw CustomError("Line " + std::to_string(line_number) + ": missing ';' in server_name");
			names.pop_back();
			std::stringstream ss(names);
			std::string name;
			while (ss >> name)
				server.addServerName(name);
		}
		else if (line.compare(0, 20, "client_max_body_size") == 0) {
			std::string size_str = trim(line.substr(20));
			if (size_str.back() != ';')
				throw CustomError("Line " + std::to_string(line_number) + ": missing ';' in client_max_body_size");
			size_str.pop_back();
			size_str = trim(size_str);
		
			size_t multiplier = 1;
			char suffix = size_str.back();
			if (suffix == 'k' || suffix == 'K') multiplier = 1024;
			else if (suffix == 'm' || suffix == 'M') multiplier = 1024 * 1024;
			else if (suffix == 'g' || suffix == 'G') multiplier = 1024 * 1024 * 1024;
		
			if (isalpha(suffix))
				size_str = size_str.substr(0, size_str.length() - 1);
		
			size_t size = std::stoul(size_str) * multiplier;
			server.setClientMaxBodySize(size);
		}
		else if (line.compare(0, 10, "error_page") == 0) {
			std::string rest = trim(line.substr(10));
			if (rest.back() != ';')
				throw CustomError("Line " + std::to_string(line_number) + ": missing ';' in error_page");
			rest.pop_back();
		
			std::stringstream ss(rest);
			std::string token;
			std::vector<int> codes;
			std::string path;
		
			while (ss >> token) {
				if (token[0] == '/')
					path = token;
				else
					codes.push_back(std::stoi(token));
			}
			if (path.empty() || codes.empty())
				throw CustomError("Line " + std::to_string(line_number) + ": malformed error_page");
		
			for (size_t i = 0; i < codes.size(); ++i)
				server.setErrorPage(codes[i], path);
		}
		else if (line.compare(0, 8, "location") == 0) {
			size_t path_start = line.find_first_not_of(" \t", 8);
			size_t path_end = line.find_first_of(" \t{", path_start);
			std::string loc_path = line.substr(path_start, path_end - path_start);

			if (line.find("{", path_end) == std::string::npos)
				throw CustomError("Line " + std::to_string(line_number) + ": expected '{' in location");

			Location location;
			location.path = loc_path;
			parseLocation(file, location, line_number);
			server.addLocation(location);
		}
		else {
			throw CustomError("Line " + std::to_string(line_number) + ": Unknown directive in server block: " + line);
		}
	}
}

std::vector<Server> ConfigParser::parseConfig( const std::string& filepath ) {
	std::ifstream file(filepath.c_str());
	if (!file.is_open())
		throw CustomError("Failed to open config file: " + filepath);

	std::vector<Server> servers;
	std::string line;
	int line_number = 0;

	while (std::getline(file, line)) {
		line_number++;
		line = removeComment(line);
		if (line.empty()) continue;

		line = trim(line);
		if (line == "server {") {
			Server server;
			parseServer(file, server);
			servers.push_back(server);
		} else if (!line.empty()) {
			throw CustomError("Line " + std::to_string(line_number) + ": Expected 'server {' but got '" + line + "'"); // to_string also May throw std::bad_alloc from the std::string constructor. Think
		}
	}
	return servers;
}