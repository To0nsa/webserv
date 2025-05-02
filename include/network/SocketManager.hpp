/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 16:08:45 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/01 13:41:30 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <vector>
#include <map>
#include <poll.h>
#include "Server.hpp"

class SocketManager {

	public:
		SocketManager() = delete;
		SocketManager(const std::vector<Server>& servers);
		~SocketManager();
		SocketManager(const SocketManager& other) = default;
		SocketManager& operator=(const SocketManager& other) = default;

		void run();
		class SocketError : public std::exception
		{
			private:
				std::string _msg;
			public:
				explicit SocketError(const std::string& msg);
				virtual const char* what() const throw();
		};

	private:
		std::vector<pollfd> _poll_fds;
		std::map<int, Server> _listen_map;
		std::map<int, Server> _client_map; 

		void setupSockets(const std::vector<Server>& servers);
		void handleNewConnection(int listen_fd);
		void handleClientData(int client_fd, size_t index);
};
