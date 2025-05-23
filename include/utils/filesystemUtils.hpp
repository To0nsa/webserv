/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   filesystemUtils.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/13 09:38:32 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/23 11:24:51 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "core/Location.hpp"
#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <filesystem>

bool         isFile(const std::string& path);
HttpResponse serveFile(const std::string& file_path, const HttpRequest& request,
                       std::string content_type = "");
std::string  detectMimeType(const std::string& file_path);
std::string  normalizePath(const std::string& path);
std::string  joinPath(const std::string& base, const std::string& suffix);
std::string  buildFilePath(const HttpRequest& request, const Location& loc);
bool         ensureDirectoryExists(const std::string& path);

bool isAbsolutePath(const std::string& pathStr);
bool containsTraversal(const std::string& pathStr);
bool containsSlash(const std::string& str);
bool isSuspiciousFilename(const std::string& pathStr);
