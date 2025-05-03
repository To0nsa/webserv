/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PrintInfo.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 14:01:22 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/03 14:08:09 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "utils/PrintInfo.hpp"

void print_usage( void )
{
	std::cout << "\n=========USAGE=========" << std::endl;
	std::cout << "  ./webserv            # Uses default.conf" << std::endl;
	std::cout << "  ./webserv config.conf" << std::endl;
}

void print_config( Config& config )
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

			std::cout << "  location " << loc.getPath() << ":" << std::endl;
			std::cout << "      root: " << loc.getRoot() << std::endl;
			std::cout << "      index: " << loc.getIndex() << std::endl;
			std::cout << "      autoindex: " << (loc.isAutoindexEnabled() ? "on" : "off") << std::endl;

			// Methods
			std::cout << "    methods: ";
			if (loc.getMethods().empty())
				std::cout << "(none)";
			else {
				for (std::set<std::string>::const_iterator it = loc.getMethods().begin(); it != loc.getMethods().end(); ++it)
					std::cout << *it << " ";
			}
			std::cout << std::endl;

			// Redirection
			if (loc.hasRedirect()) {
				std::cout << "    redirect: " << loc.getRedirect();
				if (loc.getReturnCode())
					std::cout << " (code " << loc.getReturnCode() << ")";
				std::cout << std::endl;
			}

			// Uploads
			if (loc.isUploadEnabled()) {
				std::cout << "    upload_store: " << loc.getUploadStore() << std::endl;
			}

			// CGI
			if (!loc.getCgiExtension().empty()) {
				std::cout << "    cgi_pass: " << loc.getCgiExtension() << std::endl;
			}
		}
	}
}
