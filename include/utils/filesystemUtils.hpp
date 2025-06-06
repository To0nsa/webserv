/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   filesystemUtils.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/13 09:38:32 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/06 13:35:04 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "http/HttpResponse.hpp" // for HttpResponse
#include <string>                // for string, allocator
#include <time.h>                // for time_t

class HttpRequest;
class Location;

std::string  make_temp_name(const std::string& prefix, unsigned& counter);
bool         isFile(const std::string& path);
HttpResponse serveFile(const std::string& file_path, const HttpRequest& request,
                       std::string content_type = "");
std::string  detectMimeType(const std::string& file_path);
std::string  normalizePath(const std::string& path);
std::string  joinPath(const std::string& base, const std::string& suffix);
std::string  buildFilePath(const HttpRequest& request, const Location& loc);
bool         mkdirRecursive(const std::string& path);
std::string  decodePercentEncoding(const std::string& encoded);
bool         isSymlink(const std::string& path);
time_t       getCurrentTime();

/* bool isInvalidAbsolutePath(const std::string& pathStr);
bool isSuspiciousFilename(const std::string& pathStr); */
