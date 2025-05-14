/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   buildFilePath.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/14 16:13:00 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/14 18:06:43 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <string>
#include "core/Server.hpp"
#include "core/Location.hpp"
#include "http/HttpRequest.hpp"

std::string buildFilePath(const HttpRequest& request, const Location& loc);
