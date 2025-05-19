/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_post.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/19 10:21:00 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/19 10:24:13 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "http/HttpResponseBuilder.hpp"
#include "utils/buildFilePath.hpp"
#include "utils/filesystemUtils.hpp"
#include <string.h>
#include <chrono>
#include <fstream>

HttpResponse handlePost(const HttpRequest& request, const Server& server, const Location& loc);
