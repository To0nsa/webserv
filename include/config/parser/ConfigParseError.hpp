/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParseError.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/06 13:13:44 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/21 13:43:56 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <exception>
#include <string>

class ConfigParseError : public std::exception {
  public:
    ConfigParseError(const std::string& message, const std::string& context = "");

    // explicit ConfigParseError(std::string&& fullMessage);

    const char* what() const noexcept override;

  protected:
    std::string _context;     ///< Optional contextual line
    std::string _fullMessage; ///< Formatted error with context
};

using SyntaxError     = ConfigParseError;
using UnexpectedToken = ConfigParseError;
using TokenizerError  = ConfigParseError;
using ValidationError = ConfigParseError;
