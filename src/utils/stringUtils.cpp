/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   stringUtils.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 20:09:37 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 19:41:44 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    stringUtils.cpp
 * @brief   Utility functions for string parsing, formatting, and case conversion.
 *
 * @details Implements the @ref string_utils helpers used across Webserv for:
 *          - Parsing integers and byte sizes from configuration strings with rich error reporting.
 *          - Converting strings to lowercase or uppercase.
 *          - Formatting byte counts into human-readable units.
 *          - Joining lists of strings with a delimiter.
 *          - Trimming whitespace from strings.
 *
 *          These functions are used in configuration parsing, logging, formatting
 *          HTTP responses, and general-purpose string handling.
 *
 * @ingroup string_utils
 *
 * @note Parsing helpers (`parseInt`, `parseByteSize`) throw @ref ConfigParseError
 *       with contextual information for configuration errors.
 * @warning None of these functions are locale-sensitive beyond standard
 *          `std::tolower` / `std::toupper` behavior, and they do not perform
 *          Unicode normalization.
 */

#include "utils/stringUtils.hpp"
#include "config/parser/ConfigParseError.hpp" // for ConfigParseError
#include "utils/errorUtils.hpp"               // for formatError
#include <algorithm>                          // for transform
#include <cctype>                             // for tolower, toupper
#include <charconv>                           // for from_chars, from_chars...
#include <sstream>                            // for basic_ostream, basic_o...
#include <system_error>                       // for errc
#include <vector>                             // for vector

/**
 * @ingroup string_utils
 * @brief Parses a non-negative integer from a string with context-aware error reporting.
 *
 * @details Attempts to parse the entire @p value string into a non-negative integer
 *          using `std::from_chars` for efficient, allocation-free conversion.
 *          If parsing fails, the string contains extra characters, or the result
 *          is negative, a @ref ConfigParseError is thrown. The error includes:
 *            - The field name for context.
 *            - The offending value.
 *            - The line and column numbers.
 *            - Additional context from @p context_provider, such as a snippet of the
 *              configuration file being parsed.
 *
 * @param value             String representation of the integer to parse.
 * @param field             Name of the configuration field being parsed.
 * @param line              Line number in the configuration source.
 * @param column            Column number in the configuration source.
 * @param context_provider  Callable returning a string snippet or context for diagnostics.
 * @return Parsed integer value (guaranteed to be non-negative).
 *
 * @throws ConfigParseError If parsing fails, if extra characters remain after parsing,
 *                          or if the parsed value is negative.
 *
 * @note This function is typically used when reading numeric configuration values
 *       (e.g., port numbers, limits) that must be whole, non-negative integers.
 */
int parseInt(const std::string& value, const std::string& field, int line, int column,
             const std::function<std::string()>& context_provider) {
    int result = 0;

    // Try to convert the full string to an integer using from_chars (no allocations, fast)
    auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);

    // Check if conversion failed, or if there were leftover characters, or if result is negative
    if (ec != std::errc() || ptr != value.data() + value.size() || result < 0) {
        // Throw an error with precise source location and contextual snippet for diagnostics
        throw ConfigParseError(
            formatError("Invalid value for '" + field + "': " + value, line, column),
            context_provider());
    }

    return result;
}

/**
 * @ingroup string_utils
 * @brief Parses a human-readable byte size string into a byte count.
 *
 * @details Converts a size string (e.g., `"10K"`, `"5M"`, `"2G"`, or raw bytes `"512"`)
 *          into a `std::size_t` representing the number of bytes.
 *          The suffix, if present, is case-insensitive and supports:
 *            - `K` = kibibytes (× 1024)
 *            - `M` = mebibytes (× 1024²)
 *            - `G` = gibibytes (× 1024³)
 *          Throws a @ref ConfigParseError if:
 *            - The value is empty.
 *            - Parsing fails or extra non-numeric characters remain.
 *            - The computed size exceeds the 4 GiB hard limit from the Webserv spec.
 *
 * @param value             String containing the size to parse (may include suffix).
 * @param field             Name of the configuration field being parsed.
 * @param line              Line number in the configuration source.
 * @param column            Column number in the configuration source.
 * @param context_provider  Callable returning a string snippet or context for diagnostics.
 * @return Parsed size in bytes.
 *
 * @throws ConfigParseError If the string is empty, cannot be parsed as a size, contains
 *                          leftover characters, or exceeds the maximum allowed size.
 *
 * @note Used primarily to parse configuration directives like `client_max_body_size`.
 */
std::size_t parseByteSize(const std::string& value, const std::string& field, int line, int column,
                          const std::function<std::string()>& context_provider) {
    if (value.empty()) {
        throw ConfigParseError(formatError("Empty size for '" + field + "'", line, column),
                               context_provider());
    }

    char        suffix      = value.back();
    std::string number_part = value;
    std::size_t multiplier  = 1;

    if (suffix == 'k' || suffix == 'K') {
        multiplier = 1024;
        number_part.pop_back();
    } else if (suffix == 'm' || suffix == 'M') {
        multiplier = 1024 * 1024;
        number_part.pop_back();
    } else if (suffix == 'g' || suffix == 'G') {
        multiplier = 1024ULL * 1024ULL * 1024ULL;
        number_part.pop_back();
    }

    std::size_t number = 0;
    auto [ptr, ec] =
        std::from_chars(number_part.data(), number_part.data() + number_part.size(), number);

    if (ec != std::errc() || ptr != number_part.data() + number_part.size()) {
        throw ConfigParseError(
            formatError("Invalid size format for '" + field + "': " + value, line, column),
            context_provider());
    }

    constexpr std::size_t MAX_BODY_SIZE_LIMIT = 4ULL * 1024 * 1024 * 1024;
    if (number > MAX_BODY_SIZE_LIMIT / multiplier) {
        throw ConfigParseError(
            formatError("client_max_body_size exceeds maximum allowed (4GiB)", line, column),
            context_provider());
    }

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

std::string toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

std::string formatBytes(std::size_t bytes) {
    const char* suffixes[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double      size       = static_cast<double>(bytes);
    int         i          = 0;

    while (size >= 1024.0 && i < 4) {
        size /= 1024.0;
        ++i;
    }

    std::ostringstream oss;
    oss.precision(2);
    oss << std::fixed << size << ' ' << suffixes[i];
    return oss.str();
}

std::string joinStrings(const std::vector<std::string>& list, const std::string& delim) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < list.size(); ++i) {
        oss << list[i];
        if (i + 1 < list.size())
            oss << delim;
    }
    return oss.str();
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos || end == std::string::npos)
               ? ""
               : s.substr(start, end - start + 1);
}
