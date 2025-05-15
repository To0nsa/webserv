/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_get.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/15 12:41:46 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/15 13:06:42 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <algorithm>
#include <iostream>
#include <dirent.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include "utils/buildFilePath.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "utils/filesystemUtils.hpp"

HttpResponse handleGet(const HttpRequest&, const Server&, const Location&);
