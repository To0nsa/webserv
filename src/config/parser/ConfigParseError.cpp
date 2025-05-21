/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParseError.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 20:44:40 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/21 13:44:01 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/parser/ConfigParseError.hpp"
#include <exception>
#include <iostream>

ConfigParseError::ConfigParseError(const std::string& message, const std::string& context)
    : _context(context) {
    if (_context.empty())
        _fullMessage = message;
    else
        _fullMessage = message + "\n→ " + _context;
}

/* ConfigParseError::ConfigParseError(std::string&& fullMessage)
        : _context(""), _fullMessage(std::move(fullMessage)) {} */

const char* ConfigParseError::what() const noexcept {
    return _fullMessage.c_str();
}
