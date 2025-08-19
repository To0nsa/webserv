/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   urlUtils.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/15 22:56:08 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 19:41:13 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    urlUtils.cpp
 * @brief   URL and form-encoding helpers.
 *
 * @details Implements percent-decoding (RFC 3986), `application/x-www-form-urlencoded`
 *          decoding (treating '+' as space), simple key/value parsing for form bodies,
 *          and a safe filename extractor from a URI segment. These utilities are used
 *          during request parsing and upload handling to turn encoded inputs into
 *          validated, safe strings.
 *
 * @ingroup url_utils
 */

#include <cctype>        // for isxdigit
#include <cstddef>       // for size_t
#include <regex>         // for regex_match, regex
#include <sstream>       // for basic_ostream, basic_ostringstream, operator<<
#include <stdexcept>     // for invalid_argument
#include <string>        // for allocator, char_traits, string, basic_string
#include <unordered_map> // for unordered_map
#include <utility>       // for move

/**
 * @brief Decodes percent-encoded octets in a string.
 *
 * @details Scans the input for sequences of the form `%HH` where `H` is a hex digit,
 *          converts each pair to a single byte, and returns the decoded result.
 *          Leaves all non-encoded characters unchanged.
 *
 * @ingroup url_utils
 *
 * @param encoded Input possibly containing percent-encoded bytes.
 * @return Decoded string with `%HH` sequences replaced by their byte values.
 *
 * @throws std::invalid_argument If a `%` is incomplete at the end of the string or
 *                               if the two following characters are not hex digits.
 * @note This function does not perform UTF‑8 validation; it operates on bytes.
 */
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

/**
 * @brief Decodes `application/x-www-form-urlencoded` field content.
 *
 * @details First replaces `+` with a space (per form-url-encoded rules), then applies
 *          percent-decoding to `%HH` sequences. This is suitable for decoding both
 *          keys and values extracted from a form body.
 *
 * @ingroup url_utils
 *
 * @param input Raw form field string (may include `+` and `%HH`).
 * @return Decoded string.
 *
 * @throws std::invalid_argument Propagated from @ref decodePercentEncoding on invalid encodings.
 */
std::string percentDecodeForm(const std::string& input) {
    std::string temp;
    temp.reserve(input.size());
    for (char c : input) {
        temp.push_back(c == '+' ? ' ' : c);
    }

    return decodePercentEncoding(temp);
}

/**
 * @brief Parses an `application/x-www-form-urlencoded` body into key/value pairs.
 *
 * @details Splits the body on `&`, then splits each pair on the first `=`.
 *          Both key and value are decoded using @ref percentDecodeForm. Empty pairs
 *          are ignored; missing `=` results in the pair being skipped.
 *
 * @ingroup url_utils
 *
 * @param body Full form body string (e.g., `"a=1&b=two+words"`).
 * @return Map of decoded keys to decoded values. Later duplicates will not overwrite
 *         earlier ones due to `emplace`; adjust if you want overwrite semantics.
 *
 * @throws std::invalid_argument Propagated from @ref percentDecodeForm (invalid `%HH`).
 * @note If you expect repeated keys, consider using `std::unordered_multimap` instead.
 */
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

/**
 * @brief Extracts a safe filename from the last URI segment.
 *
 * @details Takes the substring after the last `/` in @p uri, attempts percent-decoding,
 *          and validates the result against a conservative allowlist. Rejects empty,
 *          too-long (>256), or suspicious names (contains `/`, equals `"."` or `".."`,
 *          starts with `.` or `-`, or fails the regex `^[a-zA-Z0-9._-]+$`).
 *
 * @ingroup url_utils
 *
 * @param uri Source URI or path-like string.
 * @return A validated filename; returns an empty string if decoding fails or the
 *         candidate does not pass validation.
 *
 * @note Designed for deriving a download/upload filename from a URI segment without
 *       risking directory traversal or confusing special names.
 */
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

    // Enforce strict pattern
    static const std::regex safePattern(R"(^[a-zA-Z0-9._-]+$)");
    if (!std::regex_match(filename, safePattern))
        return "";

    return filename;
}
