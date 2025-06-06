/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   filesystemUtils.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/13 09:38:32 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/06 11:10:41 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "core/Location.hpp"
#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>

std::string make_temp_name(const std::string& prefix, unsigned& counter);
bool        isFile(const std::string& path);
std::string normalizePath(const std::string& path);
std::string joinPath(const std::string& base, const std::string& suffix);
std::string buildFilePath(const HttpRequest& request, const Location& loc);
bool        mkdirRecursive(const std::string& path);
std::string decodePercentEncoding(const std::string& encoded);
bool        isSymlink(const std::string& path);
std::string resolvePhysicalPath(const HttpRequest& req, const Location& loc);
std::string extractFileName(const std::string& uri);

/* bool isInvalidAbsolutePath(const std::string& pathStr);
bool isSuspiciousFilename(const std::string& pathStr); */
