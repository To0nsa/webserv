/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   methodsHandler.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/05 10:46:53 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/19 10:23:35 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    methodsHandler.hpp
 * @brief   Declares HTTP method handlers for GET, POST, and DELETE.
 *
 * @details This header defines the public entrypoints for handling
 *          HTTP request methods within Webserv. Each function maps
 *          an incoming @ref HttpRequest to an @ref HttpResponse,
 *          using the active @ref Server context and the matched
 *          @ref Location configuration.
 *
 *          Supported methods:
 *          - **GET**: Serves static files or generates autoindex
 *            listings (see @ref generateAutoindex).
 *          - **POST**: Handles uploads (raw body, URL-encoded,
 *            multipart forms).
 *          - **DELETE**: Removes existing files if permitted.
 *
 *          Helper:
 *          - @ref generateAutoindex: builds a directory listing
 *            response in HTML.
 *
 * @ingroup request_handler
 */

#pragma once

#include <string>

class Server;
class Location;
class HttpRequest;
class HttpResponse;

HttpResponse handleGet(const HttpRequest&, const Server&, const Location&);
HttpResponse generateAutoindex(const std::string& filepath, const std::string& uri,
                               const HttpRequest& request, const Server& server);

HttpResponse handlePost(const HttpRequest&, const Server&, const Location&);
HttpResponse handleMultipartForm(const HttpRequest& request, const Server& server,
                                 const std::string& fullDirPath);

HttpResponse handleDelete(const HttpRequest&, const Server&, const Location&);
