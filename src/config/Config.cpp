/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 10:36:29 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/02 13:35:46 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Config.hpp"

Config :: Config( void ) { }

Config :: ~Config( void ) { }

void Config :: addServer( const Server& server ) {
	_servers.push_back(server);
}
const std::vector<Server>& Config :: getServers( void ) const {
	return _servers;
}