/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponseBuilder.hpp                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:02:21 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/02 17:33:23 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include <string>

namespace MessageHandler {
std::string getDefaultMessage(int status_code);
}

namespace ResponseBuilder {
HttpResponse generateSuccess(int status_code, const std::string& body,
                             const std::string& content_type, const HttpRequest& request);
HttpResponse generateSuccessFile(int status_code, const std::string& file_path,
                            const std::string& content_type, const HttpRequest& request, std::streamsize content_length, std::streamsize cgiBodyOffset = 0);
HttpResponse generateError(int status_code, const Server& server, const HttpRequest& request);
HttpResponse generateRedirect(int status_code, const std::string& location,
                              const HttpRequest& request);
} // namespace ResponseBuilder
