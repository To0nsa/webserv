/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   responseBuilder.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:02:21 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/17 12:18:09 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

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
