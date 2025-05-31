/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 10:36:29 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/21 14:59:06 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/Config.hpp"
#include "config/parser/ConfigParseError.hpp"

//////////////////
// --- Public API

void Config::addServer(const Server& server) {
    _servers.push_back(server);
}

std::vector<Server>& Config::getServers() {
    return _servers;
}

const std::vector<Server>& Config::getServers() const {
    return _servers;
}
