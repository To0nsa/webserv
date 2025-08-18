/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParseError.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/18 12:05:00 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 12:01:48 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    ConfigParseError.cpp
 * @brief   Implements configuration error construction and reporting.
 *
 * @details Formats error messages (optionally including a contextual source
 *          snippet) and exposes them through `what()`.
 *
 * @ingroup config_parse_error
 */

#include "config/parser/ConfigParseError.hpp"
#include <exception>
#include <iostream>

/**
 * @brief Builds a formatted error message with optional context.
 *
 * @details If `context` is provided, appends it on a new line prefixed with
 *          an arrow (`"→ "`), producing a compact, readable diagnostic.
 *
 * @param message  Primary error message.
 * @param context  Optional offending line or snippet (may be empty).
 * @ingroup config_parse_error
 */
ConfigParseError::ConfigParseError(const std::string& message, const std::string& context)
    : _context(context) {
    if (_context.empty())
        _fullMessage = message;
    else
        _fullMessage = message + "\n→ " + _context;
}

/**
 * @brief Returns the formatted error string.
 *
 * @return Null-terminated C string owned by the exception.
 * @ingroup config_parse_error
 */
const char* ConfigParseError::what() const noexcept {
    return _fullMessage.c_str();
}
