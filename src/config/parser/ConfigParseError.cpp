/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParseError.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 20:44:40 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/17 12:05:07 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/parser/ConfigParseError.hpp"

ConfigParseError::ConfigParseError(const std::string& message, const std::string& context)
    : _context(context) {
    if (_context.empty())
        _fullMessage = message;
    else
        _fullMessage = message + "\n→ " + _context;
}

const char* ConfigParseError::what() const noexcept {
    return _fullMessage.c_str();
}
