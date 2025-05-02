/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RunCGI.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/01 19:34:27 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/01 20:50:48 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Server.hpp"
#include <string>

// Executes the CGI script located at script_path
// Returns the output from the script as a string
std::string runCGI(const std::string& script_path, const HttpRequest& request, const Server& server);