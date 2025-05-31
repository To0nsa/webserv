/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   printInfo.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 14:01:52 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/20 21:45:14 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include "config/Config.hpp"
#include "core/Location.hpp"
#include "core/Server.hpp"
#include <iostream>

std::string printUsage(void);
void        printConfig(Config& config);
