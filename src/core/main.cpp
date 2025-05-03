/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:11:30 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/03 14:16:51 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include "network/SocketManager.hpp"
#include "utils/PrintInfo.hpp"



int	main(int ac, char** av)
{
	std::string config_file;

	if (ac == 1)
		config_file = "./configs/default.conf";
	else if (ac == 2)
		config_file = av[1];
	else
	{
		print_usage();
		return(0);
	}

	try {
		// Manually define a server. We remove it as soon as we have a parser.
		// This is just a test to see if the server can be created and run.
		Server server;
		server.setHost("127.0.0.1");
		server.setPort(8080);
		server.addServerName("example.com");
		server.setClientMaxBodySize(1000000);
		server.setErrorPage(404, "/errors/404.html");

		// Define a location block
		Location loc;
		loc.setPath("/");
		loc.setRoot("/var/www/html");
		loc.setAutoindex(false);
		loc.setIndex("index.html");
		loc.addMethod("GET");
		loc.addMethod("POST");

		// Add location to server
		server.addLocation(loc);

		// Optional: another location
		Location uploadLoc;
		uploadLoc.setPath("/uploads");
		uploadLoc.setRoot("/var/www/uploads");
		uploadLoc.setAutoindex(true);
		uploadLoc.addMethod("POST");
		uploadLoc.setUploadStore("/var/www/uploads");

		server.addLocation(uploadLoc);

		Config config;
		config.addServer(server);
		// Print the configuration
		print_config(config);

		std::vector<Server> all_servers = config.getServers();
		SocketManager manager(all_servers);
		manager.run();
	} catch (const std::exception& e) {
		std::cerr << "Unexpected error: " << e.what() << std::endl;
		return (1);
	}

	return (0);
}