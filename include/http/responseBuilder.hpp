/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   responseBuilder.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:02:21 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/19 09:32:32 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    responseBuilder.hpp
 * @brief   Declarations of helper functions for constructing HTTP responses.
 *
 * @details Defines the `ResponseBuilder` namespace, which provides a set of
 *          high-level factory functions for creating different types of
 *          @ref HttpResponse objects:
 *          - @ref generateSuccess : Builds a success response with an inline body.
 *          - @ref generateSuccessFile : Builds a success response backed by a file on disk.
 *          - @ref generateError : Builds an error response using a custom error page
 *            if available, or a default generated HTML fallback.
 *          - @ref generateRedirect : Builds a redirect response with a `Location` header.
 *
 *          Also defines the `MessageHandler` namespace with
 *          @ref getDefaultMessage, a helper that maps HTTP status codes to
 *          their standard reason phrases.
 *
 *          These functions are typically used by method handlers (GET/POST/DELETE/CGI)
 *          to generate complete responses ready for serialization and transmission
 *          by the networking layer.
 *
 * @ingroup http
 */

#pragma once

#include "http/HttpResponse.hpp" // for HttpResponse
#include <iosfwd>                // for streamsize
#include <string>                // for string
class HttpRequest;
class Server;

namespace MessageHandler {
std::string getDefaultMessage(int status_code);
}

namespace ResponseBuilder {
HttpResponse generateSuccess(int status_code, const std::string& body,
                             const std::string& content_type, const HttpRequest& request);
HttpResponse generateSuccessFile(int status_code, const std::string& file_path,
                                 const std::string& content_type, const HttpRequest& request,
                                 std::streamsize content_length, std::streamsize cgiBodyOffset = 0);
HttpResponse generateError(int status_code, const Server& server, const HttpRequest& request);
HttpResponse generateRedirect(int status_code, const std::string& location,
                              const HttpRequest& request);
} // namespace ResponseBuilder
