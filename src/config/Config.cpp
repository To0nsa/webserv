/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 10:36:29 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/17 12:16:36 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/Config.hpp"

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
