/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_post.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/19 10:21:00 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/21 22:59:12 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "http/HttpResponseBuilder.hpp"
#include "utils/filesystemUtils.hpp"
#include <chrono>
#include <fstream>
#include <string.h>

HttpResponse handlePost(const HttpRequest& request, const Server& server, const Location& loc);
