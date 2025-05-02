/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 10:31:52 by irychkov          #+#    #+#             */
/*   Updated: 2025/04/30 11:08:21 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include "Config.hpp"
#include "Server.hpp"
#include "Location.hpp"

int	main(int ac, char** av)
{
	std::string config_file;

	if (ac == 1)
		config_file = "default.conf";
	else if (ac == 2)
		config_file = av[1];
	else
	{
		std::cout << "\n=========USAGE=========" << std::endl;
		std::cout << "  ./webserv            # Uses default.conf" << std::endl;
		std::cout << "  ./webserv config.conf" << std::endl;
		return(0);
	}

	// here I would parse the config file
	/* ConfigParser parser;
	Config config;

	std::vector<Server> servers = parser.parseConfig(config_file);
	for (size_t i = 0; i < servers.size(); ++i) {
		config.addServer(servers[i]);
	} */
	// and populate the Config object with servers and locations
	// For demonstration, we create a Config object manually
	Config config;

	Server server1;
	server1.setPort(8080);
	server1.setHost("127.0.0.1");
	server1.addServerName("example.com");

	Location loc1;
	loc1.path = "/";
	loc1.root = "/var/www/html";
	loc1.autoindex = false;
	server1.addLocation(loc1);

	config.addServer(server1);

	Server server2;
	server2.setPort(8081);
	server2.setHost("0.0.0.0");
	server2.addServerName("test.com");

	Location loc2;
	loc2.path = "/uploads";
	loc2.root = "/var/www/uploads";
	loc2.autoindex = true;
	server2.addLocation(loc2);

	config.addServer(server2);

	// Print the configuration
	const std::vector<Server>& servers = config.getServers();
	for (size_t i = 0; i < servers.size(); ++i) {
		std::cout << "Server " << i + 1 << ": "
				<< servers[i].getHost() << ":"
				<< servers[i].getPort() << std::endl;

		const std::vector<std::string>& names = servers[i].getServerNames();
		for (size_t j = 0; j < names.size(); ++j) {
			std::cout << "  server_name: " << names[j] << std::endl;
		}

		const std::vector<Location>& locations = servers[i].getLocations();
		for (size_t k = 0; k < locations.size(); ++k) {
			std::cout << "  location: " << locations[k].path
					  << " -> root: " << locations[k].root
					  << ", autoindex: ";
			if (locations[k].autoindex)
				std::cout << "on";
			else
				std::cout << "off";
			std::cout << std::endl;
		}
	}

	return (0);
}
