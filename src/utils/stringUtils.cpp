/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   stringUtils.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 20:09:37 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/07 12:01:13 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    stringUtils.cpp
 * @brief   Implements utility functions for parsing integers and byte sizes.
 *
 * @details Provides robust `parseInt` and `parseByteSize` helpers with
 * contextual error reporting for configuration parsing.
 * @ingroup StringUtils
 */

#include "utils/stringUtils.hpp"
#include <charconv>
#include <stdexcept>

int parseInt(const std::string& value) {
    // Wrapper with default context: no field name, unknown location, empty diagnostic
    return parseInt(value, "?", -1, -1, []() { return ""; });
}

std::size_t parseByteSize(const std::string& value) {
    // Wrapper with default context: no field name, unknown location, empty diagnostic
    return parseByteSize(value, "?", -1, -1, []() { return ""; });
}

int parseInt(const std::string& value, const std::string& field, int line, int column,
             const std::function<std::string()>& context_provider) {
    int result = 0;

    // Try to convert the full string to an integer using from_chars (no allocations, fast)
    auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);

    // Check if conversion failed, or if there were leftover characters, or if result is negative
    if (ec != std::errc() || ptr != value.data() + value.size() || result < 0) {
        // Throw an error with precise source location and contextual snippet for diagnostics
        throw std::invalid_argument("Line " + std::to_string(line) + ", column " +
                                    std::to_string(column) + ": Invalid number for '" + field +
                                    "': " + value + "\n  --> " + context_provider());
    }

    return result;
}

std::size_t parseByteSize(const std::string& value, const std::string& field, int line, int column,
                          const std::function<std::string()>& context_provider) {
    // Reject empty strings immediately for safety with pop_back
    if (value.empty()) {
        throw std::invalid_argument("Line " + std::to_string(line) + ", column " +
                                    std::to_string(column) + ": Empty size for '" + field +
                                    "'\n  --> " + context_provider());
    }

    // Extract optional size suffix ('K', 'M', 'G') from the last character
    char        suffix      = value.back();
    std::string number_part = value; // Copy of the string for potential suffix removal
    std::size_t multiplier  = 1;     // Default multiplier (bytes)

    // Normalize suffix (case-insensitive) and set multiplier
    if (suffix == 'k' || suffix == 'K') {
        multiplier = 1024;
        number_part.pop_back(); // Remove the suffix (last character)
    } else if (suffix == 'm' || suffix == 'M') {
        multiplier = 1024 * 1024;
        number_part.pop_back();
    } else if (suffix == 'g' || suffix == 'G') {
        multiplier = 1024ULL * 1024ULL * 1024ULL;
        number_part.pop_back();
    }

    // Attempt to parse the numeric portion using from_chars (efficient, non-allocating)
    std::size_t number = 0;
    auto [ptr, ec] =
        std::from_chars(number_part.data(), number_part.data() + number_part.size(), number);

    // Reject if parsing failed or did not consume the entire input
    if (ec != std::errc() || ptr != number_part.data() + number_part.size()) {
        throw std::invalid_argument("Line " + std::to_string(line) + ", column " +
                                    std::to_string(column) + ": Invalid size format for '" + field +
                                    "': " + value + "\n  --> " + context_provider());
    }

    // Return total size in bytes
    return number * multiplier;
}

std::string toLower(const std::string& str) {
    std::string result = str; // Create a mutable copy of the input string

    // Transform each character in 'result' to lowercase in-place
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return std::tolower(c); // Convert character to lowercase using locale rules
    });

    return result; // Return the transformed string
}
