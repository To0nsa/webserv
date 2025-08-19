/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   requestRouter.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/12 23:11:50 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/19 10:07:35 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

/**
 * @file HttpRequestHandler.hpp
 * @brief Declares the main entry point for handling HTTP requests.
 *
 * @details This header provides the function interface for processing
 *          a parsed HTTP request against a given server configuration.
 *          The function applies routing logic, executes the appropriate
 *          HTTP method handler (GET, POST, DELETE, etc.), and generates
 *          the corresponding HttpResponse.
 *
 * @ingroup request_handler
 */

class Server;       ///< Forward declaration of the Server class.
class HttpRequest;  ///< Forward declaration of the HttpRequest class.
class HttpResponse; ///< Forward declaration of the HttpResponse class.

HttpResponse handleRequest(const HttpRequest& request, const Server& server);
