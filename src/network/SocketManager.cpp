/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:51:20 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/08 10:52:31 by irychkov         ###   ########.fr       */
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
	for (const pollfd& pfd : _poll_fds)
		close(pfd.fd);
}

// Utility function to clean up client connections
void SocketManager::cleanupClient(int client_fd, size_t index) {
	_poll_fds.erase(_poll_fds.begin() + index);
	_client_info.erase(client_fd);
	std::cout << "cleanupClient client fd: " << client_fd << std::endl;
}

// Utility function to clean up client connections
void SocketManager::cleanupClientConnectionClose(int client_fd, size_t index) {
	cleanupClient(client_fd, index);
	close(client_fd);
	std::cout << "We close FD(Connection: close): " << client_fd << std::endl;
}

void SocketManager::checkClientTimeouts( int client_fd, size_t index ) {
	std::cout << "We check!" << std::endl;
	time_t now = time(NULL);
	// Check if client has timed out
	if (_client_info.count(client_fd) && now - _client_info[client_fd].lastRequestTime > TIMEOUT) {
		std::cout << "Client fd " << client_fd << " timed out." << std::endl;
		cleanupClientConnectionClose(client_fd, index);
	}
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
		int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0); // Create a TCP socket
		if (fd < 0)
			throw SocketError("socket() failed: " + std::string(std::strerror(errno)));

		int opt = 1; // To tell the OS: "I want to reuse this port immediately, even if it's in TIME_WAIT
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
			close(fd);
			throw SocketError("setsockopt() failed: " + std::string(std::strerror(errno)));
		}

		//MacOS
		/* if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) { // Make socket non-blocking
			close(fd);
			throw SocketError("fcntl() failed: " + std::string(std::strerror(errno)));
		} */

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
		int n = poll(&_poll_fds[0], _poll_fds.size(), 1000); // Poll each 1sec (timeout = 1sec)
		std::cout << "Poll woke up!" << std::endl;
		if (n < 0) {
			if (errno == EINTR) { // we can try to handle signal here (A signal was caught during poll().)
				running = 0;
				continue;
			}
			throw SocketError("poll() failed: " + std::string(std::strerror(errno)));
		}

		for (size_t i = _poll_fds.size(); i-- > 0;) {
			short revents = _poll_fds[i].revents;
			int current_fd = _poll_fds[i].fd;

			if (revents & POLLERR) {
				std::cout << "Socket error on fd: " << current_fd << std::endl;
				cleanupClientConnectionClose(current_fd, i);
				continue;
			}

			if (revents & POLLHUP) {
				std::cout << "Client disconnected (POLLHUP) on fd: " << current_fd << std::endl;
				cleanupClientConnectionClose(current_fd, i);
				continue;
			}
			if (revents & POLLIN) {												// Ready to read (incoming data or connection)
				if (_listen_map.count(current_fd))
					handleNewConnection(current_fd);							// Accept a new client
				else {
					std::string response = handleClientData(current_fd, i);		// Handle data from existing client
					if (response == "")
						continue;
					_client_info[current_fd].responses.push(response);						// Store the response for later
					// After handling the request, mark the socket as ready for writing (POLLOUT)
					_poll_fds[i].events |= POLLOUT;									// Mark the socket for writing
				}
			}

			if (revents & POLLOUT) {												// Ready to write (can send data)
				if (!_client_info[current_fd].responses.empty()) {
					std::string response = _client_info[current_fd].responses.front();	// Retrieve the next response
					sendResponse(current_fd, i, response);							// Send the response when the socket is ready to write
				}
			}

			checkClientTimeouts( current_fd, i);
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

	ClientInfo info;
	info.client_fd = client_fd;
	info.lastRequestTime = time(NULL);
	info.keepAlive = true;
	info.serverConfig = _listen_map[listen_fd];

	_client_info[client_fd] = info;
}

// Read data from client, send fixed response, then close
std::string SocketManager::handleClientData(int client_fd, size_t index) {
	char buffer[1024];
	int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes <= 0) {
		cleanupClient(client_fd, index);
		return ""; // Think, maybe error 500.
	}
	buffer[bytes] = '\0';
	_client_info[client_fd].lastRequestTime = time(NULL);
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
	response << "Connection: keep-alive\r\n";
	response << "\r\n";
	response << body;

	std::string full_response = response.str();
	return (full_response);
}

// Accept new client and add to poll list
void SocketManager::sendResponse(int client_fd, size_t index, std::string &response) {
	send(client_fd, response.c_str(), response.size(), 0);
	std::cout << "We sent to fd:" << client_fd << std::endl;
	std::cout << response << std::endl;
	_client_info[client_fd].responses.pop(); // Remove the sent response
	// Check if the response indicates that the connection should be kept alive
	if (response.find("Connection: keep-alive") != std::string::npos) {
		std::cout << "Connection: keep-alive - keeping the connection open" << std::endl;
		// We should not close the client connection, but just reset the POLLOUT flag if needed
		_poll_fds[index].events &= ~POLLOUT; // Reset POLLOUT flag if the connection should stay open
	} else {
		// If it's not keep-alive, close the connection
		std::cout << "Connection: close - closing the connection" << std::endl;
		cleanupClientConnectionClose(client_fd, index); // Close the connection
	}
}
