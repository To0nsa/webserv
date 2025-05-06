/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:51:20 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/06 11:58:20 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "network/SocketManager.hpp"
#include <sstream> // For stringstream, we will remove it later

// Signal handler for exiting the server
static volatile sig_atomic_t running = 1;

// We don't need it. We can handle it directly in poll. Let's discuss.
static void signalHandler(int signum) {
	if (signum == SIGINT)
		running = 0;
}

// Constructor: sets up sockets for each server defined in the config
SocketManager::SocketManager(const std::vector<Server>& servers) {
	signal(SIGINT, signalHandler);
	setupSockets(servers);
}

// Destructor: closes all open file descriptors
SocketManager::~SocketManager() {
	for (size_t i = 0; i < _poll_fds.size(); ++i)
		close(_poll_fds[i].fd);
}

// Custom exception for socket errors
SocketManager::SocketError::SocketError(const std::string& msg) {
	_msg = msg;
}

const char* SocketManager::SocketError::what() const throw() {
	return (_msg.c_str());
}

// Set up sockets for each server (host:port)
void SocketManager::setupSockets(const std::vector<Server>& servers) {
	for (size_t i = 0; i < servers.size(); ++i) {
		int fd = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket
		if (fd < 0)
			throw SocketError("socket() failed: " + std::string(std::strerror(errno)));

		int opt = 1; // To tell the OS: "I want to reuse this port immediately, even if it's in TIME_WAIT
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
			close(fd);
			throw SocketError("setsockopt() failed: " + std::string(std::strerror(errno)));
		}

		if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) { // Make socket non-blocking
			close(fd);
			throw SocketError("fcntl() failed: " + std::string(std::strerror(errno)));
		}

		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(servers[i].getPort()); // Convert port to network byte order

		// Convert hostname to IP address
		if (servers[i].getHost() == "localhost")
			addr.sin_addr.s_addr = inet_addr("127.0.0.1");
		else
			addr.sin_addr.s_addr = inet_addr(servers[i].getHost().c_str());

		if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) { // Bind socket to IP:port
			close(fd);
			throw SocketError("bind() failed on " + servers[i].getHost() + ":" + std::to_string(servers[i].getPort()) + ": " + strerror(errno));
		}

		if (listen(fd, SOMAXCONN) < 0) { // Start listening for incoming connections
			close(fd);
			throw SocketError("listen() failed: " + std::string(std::strerror(errno)));
		}

		// Register fd in poll list
		_poll_fds.push_back((pollfd){ fd, POLLIN, 0 });
		_listen_map[fd] = servers[i]; // Map fd to its corresponding server

		std::cout << "Listening on " << servers[i].getHost() << ":" << servers[i].getPort() << std::endl;
	}
}

// Main server loop using poll
void SocketManager::run() {
	while (running) {
		int n = poll(&_poll_fds[0], _poll_fds.size(), -1); // Block until at least one fd is ready (timeout = -1)
		if (n < 0) {
			if (errno == EINTR) { // we can try to handle signal here (A signal was caught during poll().)
				/* running = 0; */
				continue;
			}
			throw SocketError("poll() failed: " + std::string(std::strerror(errno)));
		}

		for (size_t i = _poll_fds.size(); i-- > 0;) {
			short revents = _poll_fds[i].revents;
			int current_fd = _poll_fds[i].fd;

			if (revents & POLLERR) {
				std::cerr << "Socket error on fd: " << current_fd << std::endl;
				close(current_fd);
				_poll_fds.erase(_poll_fds.begin() + i);
				_client_map.erase(current_fd);
				continue;
			}

			if (revents & POLLHUP) {
				std::cout << "Client disconnected (POLLHUP) on fd: " << _poll_fds[i].fd << std::endl;
				close(current_fd);
				_poll_fds.erase(_poll_fds.begin() + i);
				_client_map.erase(current_fd);
				continue;
			}
			if (revents & POLLIN) { // Ready to read (incoming data or connection)
				if (_listen_map.count(_poll_fds[i].fd))
					handleNewConnection(_poll_fds[i].fd); // Accept a new client
				else
					handleClientData(_poll_fds[i].fd, i); // Handle data from existing client
			}
		}
	}
	std::cout << std::endl;
	std::cout << "Shutting down server" << std::endl;
}

// Accept new client and add to poll list
void SocketManager::handleNewConnection(int listen_fd) {
	int client_fd = accept(listen_fd, NULL, NULL);
	if (client_fd < 0)
		return; // Shall we log it?

	if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
		close(client_fd);
		return; // Shall we log it?
	}

	_poll_fds.push_back((pollfd){ client_fd, POLLIN, 0 });
	std::cout << std::endl;
	std::cout << "Accepted client on fd: " << client_fd << std::endl;

	// Store which server this client is connected to based on listen_fd
	_client_map[client_fd] = _listen_map[listen_fd];
}

// Read data from client, send fixed response, then close
void SocketManager::handleClientData(int client_fd, size_t index) {
	char buffer[1024];
	int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes <= 0) {
		close(client_fd);
		_poll_fds.erase(_poll_fds.begin() + index);
		_client_map.erase(client_fd);
		return;
	}
	buffer[bytes] = '\0';
	std::cout << std::endl;
	std::cout << "Received request: " << buffer << std::endl;
	std::cout << std::endl;

	/*==== Here we will have Request with parser ====*/
	// At this point, you could parse the request to handle HTTP methods, headers, etc.

	/* Example code (uncomment when implementing request parsing):
	HttpRequest request;
	if (!request.parse(raw_request)) {
		std::cerr << "Failed to parse HTTP request.\n";
		close(client_fd);
		_poll_fds.erase(_poll_fds.begin() + index);
		_client_map.erase(client_fd);
		return;
	}
	request.printRequest(); 
	*/

	/*==== parser ends ====*/

	/*==== Here we will have RequestHandler ====*/
	// You might want to handle the parsed request based on HTTP methods, route, etc.
	// The request handler would process the parsed request and generate an appropriate response.

	/* Example code (uncomment when implementing request handling):
	const Server& server = _client_map[client_fd];  // Get the server configuration
	RequestHandler(server, request);  // Handle the request
	*/
	
	// Temporary HTTP response logic for now (simple hardcoded response)
	std::string body = "<h1>Success</h1><p>OK</p>";
	std::stringstream response;
	response << "HTTP/1.1 200 OK\r\n";
	response << "Content-Type: text/html\r\n";
	response << "Content-Length: " << body.length() << "\r\n";
	response << "Connection: close\r\n";
	response << "\r\n";
	response << body;

	std::string full_response = response.str();
	send(client_fd, full_response.c_str(), full_response.size(), 0);
	std::cout << "We sent to fd:" << client_fd << std::endl;
	close(client_fd);
	std::cout << "We close fd:" << client_fd << std::endl;
	_poll_fds.erase(_poll_fds.begin() + index);
	_client_map.erase(client_fd);
}
