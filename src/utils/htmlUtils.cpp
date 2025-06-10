/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   htmlUtils.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/09 22:25:49 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/09 22:39:21 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <string>

// HTML-escape &, <, >, ", '
// so it's safe to embed in an HTML context.
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