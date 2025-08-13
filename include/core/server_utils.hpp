/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server_utils.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/02 20:29:58 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/13 22:40:47 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    server_utils.hpp
 * @brief   Declares server selection util for virtual host resolution.
 *
 * @details Contains function declaration for selecting the appropriate
 *          `Server` instance based on listening port and HTTP `Host` header.
 * @ingroup core
 */

#pragma once

#include "Server.hpp"
#include <string>
#include <vector>

const Server&
findMatchingServer(const std::vector<Server>& servers, int port,
                   const std::string& host_name); // documented at src/core/server_utils.cpp
