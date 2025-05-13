/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParseError.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 20:44:40 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/08 20:10:18 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    ConfigParseError.cpp
 * @brief   Implements the `ConfigParseError` exception class.
 *
 * @details This file defines the `ConfigParseError` class, which is used to handle errors
 *          encountered during the parsing of configuration files. The class extends
 *          `std::invalid_argument` and includes additional context information such
 *          as the line and column number where the error occurred, as well as the
 *          surrounding context from the configuration file. This provides detailed error
 *          reporting and helps with debugging configuration issues.
 *
 * @ingroup config
 */

#include "config/parser/ConfigParseError.hpp"
#include <iostream>

ConfigParseError::ConfigParseError(const std::string& message, std::string context)
    : std::invalid_argument(message), context_(std::move(context)) {
}
