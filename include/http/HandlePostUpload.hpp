/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HandlePostUpload.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/01 18:30:34 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/01 18:32:29 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "Server.hpp"
#include "HttpRequest.hpp"

// If upload is successful, returns full file path
// Otherwise returns a string "ERROR:<status_code>"
std::string handlePostUpload(const Server& server, const HttpRequest& request);