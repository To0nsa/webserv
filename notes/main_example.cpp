/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/29 13:42:50 by irychkov          #+#    #+#             */
/*   Updated: 2025/04/29 14:58:09 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <arpa/inet.h>

struct s_server_data {
	std::string server_name;
	int listen;
};

int	main(int ac, char **av)
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
	
	s_server_data config;
	config.server_name = "localhost";
	config.listen = 8080;

	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(8080);
	if (config.server_name == "localhost")
		serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
	else
		serverAddress.sin_addr.s_addr = INADDR_ANY;
	// binding socket.
	bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

	// listening to the assigned socket
	listen(serverSocket, 5);

	// accepting connection request
	int clientSocket = accept(serverSocket, nullptr, nullptr);

	// receiving data
	char buffer[1024] = { 0 };
	recv(clientSocket, buffer, sizeof(buffer), 0);
	std::cout << "Message from client: " << buffer << std::endl;

	std::string response =
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: text/html\r\n"
	"Content-Length: 29\r\n"
	"\r\n"
	"<h1>Hello from Webserv!</h1>";

	send(clientSocket, response.c_str(), response.size(), 0);

	close(clientSocket);
	close(serverSocket);
	

	return 0;
}
