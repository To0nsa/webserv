/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HandleDelete.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/01 19:00:17 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/01 19:01:43 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "Server.hpp"
#include "HttpRequest.hpp"

// Attempts to delete a file based on DELETE request and location config
// Returns HTTP status code as string: "200", "403", "404", "405", "500"
std::string handleDelete(const Server& server, const HttpRequest& request);