/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_get.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/15 12:41:46 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/22 21:04:15 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "http/HttpResponseBuilder.hpp"
#include "utils/filesystemUtils.hpp"
#include <algorithm>
#include <chrono>
#include <dirent.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

HttpResponse handleGet(const HttpRequest&, const Server&, const Location&);
