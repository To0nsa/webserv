/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponseBuilder.hpp                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:02:21 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/11 12:50:24 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "core/Server.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpRequest.hpp"
#include <string>

namespace MessageHandler {
	std::string getDefaultMessage(int status_code);
}

namespace ResponseBuilder {
	HttpResponse generateSuccess(int status_code, const std::string& body, const std::string& content_type, const HttpRequest& request);
	HttpResponse generateError(int status_code, const Server& server, const HttpRequest& request);
	HttpResponse generateRedirect(int status_code, const std::string& location, const HttpRequest& request);
}
