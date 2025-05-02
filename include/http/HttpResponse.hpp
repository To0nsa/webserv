/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/01 20:22:49 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/01 20:44:42 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <string>
#include <sstream>
#include <map>
#include <fstream>
#include "Server.hpp"
#include "HttpRequest.hpp"
#include "FilePath.hpp"

std::string buildResponse(int statusCode, const std::string& body, const std::string& contentType);
std::string buildErrorBody(const Server& server, int code);