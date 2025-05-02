/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 16:17:41 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/01 21:26:50 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "SocketManager.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "HandlePostUpload.hpp"
#include "HandleDelete.hpp"
#include "RunCGI.hpp"
#include "FilePath.hpp"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <arpa/inet.h>
#include <errno.h>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>

// Constructor: sets up sockets for each server defined in the config
SocketManager::SocketManager(const std::vector<Server>& servers) {
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
			throw SocketError("socket() failed: " + std::string(strerror(errno)));

		if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) { // Make socket non-blocking
			close(fd);
			throw SocketError("fcntl() failed: " + std::string(strerror(errno)));
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
			throw SocketError("listen() failed: " + std::string(strerror(errno)));
		}

		// Register fd in poll list
		_poll_fds.push_back((pollfd){ fd, POLLIN, 0 });
		_listen_map[fd] = servers[i]; // Map fd to its corresponding server

		std::cout << "Listening on " << servers[i].getHost() << ":" << servers[i].getPort() << std::endl;
	}
}

// Main server loop using poll
void SocketManager::run() {
	while (true) {
		int n = poll(&_poll_fds[0], _poll_fds.size(), -1); // Block until at least one fd is ready
		if (n < 0)
			throw SocketError("poll() failed: " + std::string(strerror(errno)));

		for (size_t i = 0; i < _poll_fds.size(); ++i) {
			if (_poll_fds[i].revents & POLLIN) { // Ready to read (incoming data or connection)
				if (_listen_map.count(_poll_fds[i].fd))
					handleNewConnection(_poll_fds[i].fd); // Accept a new client
				else
					handleClientData(_poll_fds[i].fd, i); // Handle data from existing client
			}
		}
	}
}

// Accept new client and add to poll list
void SocketManager::handleNewConnection(int listen_fd) {
	int client_fd = accept(listen_fd, NULL, NULL);
	if (client_fd < 0)
		return;

	if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
		close(client_fd);
		return;
	}

	_poll_fds.push_back((pollfd){ client_fd, POLLIN, 0 });
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
	std::cout << "Received request: " << buffer << std::endl;

	std::string raw_request(buffer);

	HttpRequest request;
	if (!request.parse(raw_request)) {
		std::cerr << "Failed to parse HTTP request.\n";
		close(client_fd);
		_poll_fds.erase(_poll_fds.begin() + index);
		_client_map.erase(client_fd);
		return;
	}
	request.printRequest();

	// Get the server that accepted this client
	const Server& server = _client_map[client_fd];

	// Check if the request method is POST and handle file upload
	if (request.getMethod() == "POST") {
		const std::string result = handlePostUpload(server, request);
		if (result.find("ERROR:") == 0) {
			std::string code = result.substr(6);
			std::string body = buildErrorBody(server, std::atoi(code.c_str()));
			std::string response_str = buildResponse(std::atoi(code.c_str()), body, "text/html");
			send(client_fd, response_str.c_str(), response_str.size(), 0);
			close(client_fd);
			_poll_fds.erase(_poll_fds.begin() + index);
			_client_map.erase(client_fd);
			return;
		} else {
			std::string success_body = "<h1>Upload successful</h1><p>Saved to: " + result + "</p>";
			std::string response_str = buildResponse(201, success_body, "text/html");
			send(client_fd, response_str.c_str(), response_str.size(), 0);
			close(client_fd);
			_poll_fds.erase(_poll_fds.begin() + index);
			_client_map.erase(client_fd);
			return;
		}
	}

	// Check if the request method is DELETE
	if (request.getMethod() == "DELETE") {
		std::string statusStr = handleDelete(server, request);
		int status = std::atoi(statusStr.c_str());

		std::string body;
		if (status != 200) {
			body = buildErrorBody(server, status);
		} else {
			body = "<h1>Deleted successfully</h1>";
		}

		std::string response_str = buildResponse(status, body, "text/html");
		send(client_fd, response_str.c_str(), response_str.size(), 0);
		close(client_fd);
		_poll_fds.erase(_poll_fds.begin() + index);
		_client_map.erase(client_fd);
		return;
	}
	

	if (request.getMethod() != "GET") {
		std::cerr << "Unsupported HTTP method: " << request.getMethod() << "\n";
		close(client_fd);
		_poll_fds.erase(_poll_fds.begin() + index);
		_client_map.erase(client_fd);
		return;
	}

	// Build the file path based on server and request
	std::string filepath = buildFilePath(server, request);
	std::cout << "Resolved file path: " << filepath << std::endl;

	// CGI handling
	const std::vector<Location>& locs = server.getLocations();
	for (size_t i = 0; i < locs.size(); ++i) {
		const Location& loc = locs[i];
		if (filepath.size() >= loc.cgi_extension.size() &&
			filepath.compare(filepath.size() - loc.cgi_extension.size(), loc.cgi_extension.size(), loc.cgi_extension) == 0) {

			std::string cgi_output = runCGI(filepath, request, server);

			std::stringstream response;
			size_t pos = cgi_output.find("\r\n\r\n");
			if (pos != std::string::npos) {
				std::string headers = cgi_output.substr(0, pos);
				std::string body = cgi_output.substr(pos + 4);
				response << "HTTP/1.1 200 OK\r\n" << headers << "\r\n\r\n" << body;
			} else {
				response << "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
				response << "Content-Length: " << cgi_output.size() << "\r\n\r\n";
				response << cgi_output;
			}

			std::string response_str = response.str();
			send(client_fd, response_str.c_str(), response_str.size(), 0);
			close(client_fd);
			_poll_fds.erase(_poll_fds.begin() + index);
			_client_map.erase(client_fd);
			return;
		}
	}

	std::ifstream file(filepath.c_str());
	std::string body;
	bool fileExists = file.good();
	bool isDirectory = false;

	// Check if path is a directory
	struct stat fileStat;
	if (stat(filepath.c_str(), &fileStat) == 0 && S_ISDIR(fileStat.st_mode)) {
		std::cout << "Path is a directory.\n";
		isDirectory = true;
	}

	if (fileExists && !isDirectory) {
		std::stringstream ss;
		ss << file.rdbuf();
		body = ss.str();
	} else if (isDirectory) {
		// Check if autoindex is enabled
		std::string uri = request.getPath();
		const std::vector<Location>& locations = server.getLocations();
		bool autoindexEnabled = false;
		for (size_t i = 0; i < locations.size(); ++i) {
			if (uri.find(locations[i].path) == 0 && locations[i].autoindex) {
				autoindexEnabled = true;
				break;
			}
		}

		if (autoindexEnabled) {
			DIR* dir = opendir(filepath.c_str());
			if (dir == NULL) {
				std::cout << "[AUTOINDEX] Failed to open directory!" << std::endl;
			}
			if (dir) {
				struct dirent* entry;
				std::stringstream indexStream;
				indexStream << "<html><body><h1>Index of " << uri << "</h1><ul>";
				while ((entry = readdir(dir)) != NULL) {
					indexStream << "<li><a href='" << entry->d_name << "'>" << entry->d_name << "</a></li>"; // Think about .. and .
				}
				indexStream << "</ul></body></html>";
				closedir(dir);
				body = indexStream.str();
			}
		}
	}

	if (body.empty()) {
		body = buildErrorBody(server, 404);
		std::string response_str = buildResponse(404, body, "text/html");
		send(client_fd, response_str.c_str(), response_str.size(), 0);
	} else {
		std::string response_str = buildResponse(200, body, "text/html");
		send(client_fd, response_str.c_str(), response_str.size(), 0);
	}
	close(client_fd);
	_poll_fds.erase(_poll_fds.begin() + index);
	_client_map.erase(client_fd);

	/* // Static response for now
	std::string body = "<h1>Hello from Webserv!</h1>";
	std::stringstream response_stream;

	response_stream << "HTTP/1.1 200 OK\r\n";
	response_stream << "Content-Type: text/html\r\n";
	response_stream << "Content-Length: " << body.size() << "\r\n";
	response_stream << "Connection: close\r\n";
	response_stream << "\r\n";
	response_stream << body;

	// Convert stringstream to string
	std::string response = response_stream.str();

	send(client_fd, response.c_str(), response.size(), 0);
	close(client_fd);
	_poll_fds.erase(_poll_fds.begin() + index); */
}
