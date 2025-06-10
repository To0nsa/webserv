/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   filesystemUtils.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/13 09:38:32 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/10 21:47:36 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "http/HttpResponse.hpp" // for HttpResponse
#include <string>                // for string, allocator
#include <time.h>                // for time_t

class HttpRequest;
class Location;

std::string make_temp_name(const std::string& prefix, unsigned& counter);
bool        isFile(const std::string& path);
std::string normalizePath(const std::string& path);
std::string joinPath(const std::string& base, const std::string& suffix);
std::string buildFilePath(const HttpRequest& request, const Location& loc);
bool        mkdirRecursive(const std::string& path);
std::string decodePercentEncoding(const std::string& encoded);
bool        isSymlink(const std::string& path);
time_t      getCurrentTime();
std::string resolvePhysicalPath(const HttpRequest& req, const Location& loc);
std::string makeSafeUploadPath(const std::string& uploadRoot, const std::string& rawFilename);
std::string sanitizeFilename(const std::string& raw);
