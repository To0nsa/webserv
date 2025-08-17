/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   urlUtils.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/06 13:22:39 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/17 21:06:11 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <cctype>        // for isxdigit
#include <cstddef>       // for size_t
#include <regex>         // for regex_match, regex
#include <sstream>       // for basic_ostream, basic_ostringstream, operator<<
#include <stdexcept>     // for invalid_argument
#include <string>        // for allocator, char_traits, string, basic_string
#include <unordered_map> // for unordered_map
#include <utility>       // for move

std::string decodePercentEncoding(const std::string& encoded) {
    std::ostringstream result;
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%') {
            if (i + 2 >= encoded.length())
                throw std::invalid_argument("Incomplete percent-encoding at end of URI");

            char hex1 = encoded[i + 1];
            char hex2 = encoded[i + 2];
            if (!std::isxdigit(static_cast<unsigned char>(hex1)) ||
                !std::isxdigit(static_cast<unsigned char>(hex2))) {
                throw std::invalid_argument(std::string{"Invalid hex in percent-encoding: %"} +
                                            hex1 + hex2);
            }

            int byte = std::stoi(encoded.substr(i + 1, 2), nullptr, 16);
            result << static_cast<char>(byte);
            i += 2;
        } else {
            result << encoded[i];
        }
    }
    return result.str();
}

std::string percentDecodeForm(const std::string& input) {
    std::string temp;
    temp.reserve(input.size());
    for (char c : input) {
        temp.push_back(c == '+' ? ' ' : c);
    }

    return decodePercentEncoding(temp);
}

std::unordered_map<std::string, std::string> parseFormUrlEncoded(const std::string& body) {
    std::unordered_map<std::string, std::string> form;
    size_t                                       start = 0;

    while (start < body.size()) {
        size_t      amp    = body.find('&', start);
        size_t      length = (amp == std::string::npos ? body.size() : amp) - start;
        std::string pair   = body.substr(start, length);

        if (!pair.empty()) {
            size_t eq = pair.find('=');
            if (eq != std::string::npos) {
                std::string key = percentDecodeForm(pair.substr(0, eq));
                std::string val = percentDecodeForm(pair.substr(eq + 1));
                form.emplace(std::move(key), std::move(val));
            }
        }

        if (amp == std::string::npos) {
            break;
        }
        start = amp + 1;
    }

    return form;
}

std::string extractFilenameFromUri(const std::string& uri) {
    std::string filename;

    // Extract last segment
    std::size_t pos = uri.find_last_of('/');
    if (pos != std::string::npos)
        filename = uri.substr(pos + 1);
    else
        filename = uri;

    // Decode percent-encoding
    try {
        filename = decodePercentEncoding(filename);
    } catch (...) {
        return ""; // invalid encoding
    }

    // Reject suspicious filenames
    if (filename.empty() || filename.size() > 256 || filename.find('/') != std::string::npos)
        return "";

    if (filename == "." || filename == ".." || filename[0] == '.' || filename[0] == '-')
        return "";

    // Optional: enforce strict pattern
    static const std::regex safePattern(R"(^[a-zA-Z0-9._-]+$)");
    if (!std::regex_match(filename, safePattern))
        return "";

    return filename;
}
