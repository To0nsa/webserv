/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:51:47 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/03 15:24:06 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    SocketManager.hpp
 * @brief   Declares the SocketManager class responsible for managing server sockets.
 *
 * @details The SocketManager sets up listening sockets, handles incoming client connections,
 * and manages client I/O using non-blocking `poll()`. It supports multiple server blocks
 * listening on different ports and performs proper cleanup on shutdown.
 * 
 * @ingroup network
 */

#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "core/Server.hpp"

/**
 * @defgroup network Networking
 * @brief Low-level socket and I/O management for HTTP servers.
 * @{
 */

/**
 * @brief Manages all network sockets for a set of HTTP servers.
 *
 * @details The SocketManager is responsible for initializing listening sockets based on
 * configured Server objects, accepting new client connections, and processing I/O events
 * via the `poll()` syscall in a non-blocking manner. It maps file descriptors to their
 * corresponding server configurations and handles each client request accordingly.
 *
 * @ingroup network
 */
class SocketManager {

	public:
		SocketManager( void ) = delete;
		/**
		 * @brief Constructs a SocketManager with the given server configurations.
		 *
		 * @param servers A vector of Server objects representing the server configurations.
		 */
		SocketManager( const std::vector<Server>& servers );
		/**
		 * @brief Destructor that closes all open file descriptors.
		 */
		~SocketManager( void );
		SocketManager( const SocketManager& other ) = default;
		SocketManager& operator=( const SocketManager& other ) = default;

		/**
		 * @brief Starts the server loop, handling I/O using poll().
		 *
		 * @details Accepts new clients and handles data from existing ones.
		 * Will exit cleanly on signal (e.g. SIGINT).
		 */
		void run();
		/**
		 * @brief Custom exception class for socket-related errors.
		 *
		 * @details Inherits from std::exception and provides a custom error message.
		 */
		class SocketError : public std::exception
		{
			private:
				std::string _msg;
			public:
				explicit SocketError(const std::string& msg);
				virtual const char* what() const throw();
		};

	private:
		std::vector<pollfd> _poll_fds;		///< Monitored file descriptors for poll().
		std::map<int, Server> _listen_map;	///< Maps listening socket fds to their corresponding server configurations.
		std::map<int, Server> _client_map;	///< Maps client fds to their corresponding server configurations.

		/**
		 * @brief Initializes all listening sockets for the provided servers.
		 *
		 * @param servers A vector of server configurations.
		 */
		void setupSockets( const std::vector<Server>& servers );
		/**
		 * @brief Accepts a new client connection and adds it to the poll list.
		 *
		 * @param listen_fd File descriptor of the listening socket.
		 */
		void handleNewConnection( int listen_fd );
		/**
		 * @brief Reads from a client socket, generates a response, and sends it.
		 *
		 * @param client_fd File descriptor of the connected client.
		 * @param index Index of the fd in the `_poll_fds` vector.
		 */
		void handleClientData( int client_fd, size_t index );
};
