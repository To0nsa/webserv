/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 10:31:52 by irychkov          #+#    #+#             */
/*   Updated: 2025/04/30 16:23:49 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include "Config.hpp"
#include "Server.hpp"
#include "Location.hpp"
#include "ConfigParser.hpp"
#include "SocketManager.hpp"

static void print_usage( void )
{
	std::cout << "\n=========USAGE=========" << std::endl;
	std::cout << "  ./webserv            # Uses default.conf" << std::endl;
	std::cout << "  ./webserv config.conf" << std::endl;
}

static void print_config( Config& config )
{
	const std::vector<Server>& servers = config.getServers();
	for (size_t i = 0; i < servers.size(); ++i) {
		const Server& server = servers[i];
	
		std::cout << "Server " << i + 1 << ": "
				<< server.getHost() << ":"
				<< server.getPort() << std::endl;
	
		// Server Names
		const std::vector<std::string>& names = server.getServerNames();
		for (size_t j = 0; j < names.size(); ++j) {
			std::cout << "  server_name: " << names[j] << std::endl;
		}
	
		// Error Pages
		const std::map<int, std::string>& errors = server.getErrorPages();
		for (std::map<int, std::string>::const_iterator it = errors.begin(); it != errors.end(); ++it) {
			std::cout << "  error_page " << it->first << ": " << it->second << std::endl;
		}
	
		// Client Body Size
		std::cout << "  client_max_body_size: " << server.getClientMaxBodySize() << std::endl;
	
		// Locations
		const std::vector<Location>& locations = server.getLocations();
		for (size_t k = 0; k < locations.size(); ++k) {
			const Location& loc = locations[k];
	
			std::cout << "  location " << loc.path << ":" << std::endl;
			std::cout << "    root: " << loc.root << std::endl;
			std::cout << "    index: " << loc.index << std::endl;
	
			std::cout << "    autoindex: " << (loc.autoindex ? "on" : "off") << std::endl;
	
			// Methods
			std::cout << "    methods: ";
			if (loc.methods.empty())
				std::cout << "(none)";
			else {
				for (size_t m = 0; m < loc.methods.size(); ++m)
					std::cout << loc.methods[m] << " ";
			}
			std::cout << std::endl;
	
			// Redirection
			if (!loc.redirect.empty()) {
				std::cout << "    redirect: " << loc.redirect;
				if (loc.return_code)
					std::cout << " (code " << loc.return_code << ")";
				std::cout << std::endl;
			}
	
			// Uploads
			if (!loc.upload_store.empty()) {
				std::cout << "    upload_store: " << loc.upload_store << std::endl;
			}
	
			// CGI
			if (!loc.cgi_extension.empty()) {
				std::cout << "    cgi_pass: " << loc.cgi_extension << std::endl;
			}
		}
	}
}

	
int	main(int ac, char** av)
{
	std::string config_file;

	if (ac == 1)
		config_file = "default.conf";
	else if (ac == 2)
		config_file = av[1];
	else
	{
		print_usage();
		return(0);
	}

	try {
		ConfigParser parser;
		Config config;

		std::vector<Server> all_servers = parser.parseConfig(config_file);
		for (size_t i = 0; i < all_servers.size(); ++i) {
			config.addServer(all_servers[i]);
		}
		// Print the configuration
		print_config(config);

		SocketManager manager(all_servers);
		manager.run();
	} catch (const ConfigParser::CustomError& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return (1);
	} catch (const std::exception& e) {
		std::cerr << "Unexpected error: " << e.what() << std::endl;
		return (1);
	}

	return (0);
}
