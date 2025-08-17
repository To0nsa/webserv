/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   errorUtils.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 21:08:50 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/17 12:30:16 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    errorUtils.cpp
 * @brief   Implements error formatting utilities.
 *
 * @details
 * Provides the implementation of `formatError`, a helper function that
 * generates consistent diagnostic messages with line and column info.
 * @ingroup ErrorUtils
 */

#include <cctype>        // for isxdigit
#include <cstddef>       // for size_t
#include <regex>         // for regex_match, regex
#include <sstream>       // for basic_ostream, basic_ostringstream, operator<<
#include <stdexcept>     // for invalid_argument
#include <string>        // for allocator, char_traits, string, basic_string
#include <unordered_map> // for unordered_map
#include <utility>       // for move

std::string formatError(const std::string& msg, int line, int column) {
    // Builds a standardized error message with line and column context
    return "Line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + msg;
}