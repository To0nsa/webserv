/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   stringUtils.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 20:09:37 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/25 21:43:03 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "utils/stringUtils.hpp"
#include "config/parser/ConfigParseError.hpp"
#include "utils/errorUtils.hpp"

#include <charconv>
#include <sstream>
#include <stdexcept>
#include <vector>

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
