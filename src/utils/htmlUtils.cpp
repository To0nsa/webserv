/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   htmlUtils.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/09 22:25:49 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/15 22:43:23 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    htmlUtils.cpp
 * @brief   Implements HTML escaping utilities for Webserv.
 *
 * @details Provides helper functions for safely embedding arbitrary strings
 *          into HTML content. These utilities perform entity escaping of
 *          special characters that would otherwise be interpreted as HTML
 *          markup, preventing injection vulnerabilities (e.g., XSS).
 *
 * @ingroup html_utils
 */

#include <string>

/**
 * @brief Escapes special HTML characters in a string.
 *
 * @details Replaces the characters `&`, `<`, `>`, `"`, and `'` with their
 *          corresponding HTML entity representations so that the output
 *          can be safely embedded in an HTML context without being parsed
 *          as markup. This is intended to prevent HTML injection and
 *          cross-site scripting (XSS) when displaying untrusted data.
 *
 * @ingroup html_utils
 *
 * @param in Input string that may contain unsafe HTML characters.
 * @return Escaped string safe for insertion into HTML content.
 *
 * @note Only escapes the five most common HTML special characters. If you
 *       need to handle other entities, extend this mapping accordingly.
 */
std::string htmlEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (unsigned char c : in) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&#39;";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}