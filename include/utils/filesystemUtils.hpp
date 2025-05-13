/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   filesystemUtils.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/13 09:38:32 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/13 09:39:23 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <string>
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

bool isDirectory(const std::string& path);
bool fileExists(const std::string& path);
HttpResponse serveFile(const std::string& file_path,
                       const HttpRequest& request,
                       std::string content_type = "");
std::string detectMimeType(const std::string& file_path);
std::string generateAutoindexHTML(const std::string& directory_path, const std::string& uri_path);

