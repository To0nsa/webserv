/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   buildFilePath.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/14 16:13:00 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/15 19:19:50 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "core/Location.hpp"
#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include <string>

std::string joinPath(const std::string& base, const std::string& suffix);
std::string buildFilePath(const HttpRequest& request, const Location& loc);
