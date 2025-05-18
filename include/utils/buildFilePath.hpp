/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   buildFilePath.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/14 16:13:00 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/18 12:38:47 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "core/Location.hpp"
#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include <string>
#include <cstring>
#include <sstream>
#include <sys/stat.h>
#include <iostream>

std::string joinPath(const std::string& base, const std::string& suffix);
std::string buildFilePath(const HttpRequest& request, const Location& loc);
bool mkdirRecursive(const std::string& path);
