/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParseError.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/06 13:13:44 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/11 09:09:48 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    ConfigParseError.hpp
 * @brief   Custom exception for configuration file parsing errors.
 *
 * @details This class represents all parsing-related exceptions that may be thrown
 * while processing a configuration file. Specific aliases like SyntaxError or
 * TokenizerError are provided for semantic clarity during parsing. 
 *
 * @note Those exceptions are caught in the main() and stop the execution of the
 * program.
 *
 * @ingroup config
 */

#pragma once

#include <exception>
#include <string>

/**
 * @brief Exception thrown when a configuration parsing error occurs.
 *
 * @details Stores an optional context line to help identify the origin of the error
 * in the configuration file provided by getLineSnippet() or extractLine().
 * The full message is available via `what()`.
 *
 * @ingroup config
 */
class ConfigParseError : public std::exception {
  public:
    /**
      * @brief Constructs a ConfigParseError.
      *
      * @param message  The main error message.
      * @param context  Optional context (source line) to append to the message.
     */
    ConfigParseError(const std::string& message, const std::string& context = "");

    const char* what() const noexcept override;

  protected:
    std::string _context;     ///< Optional contextual line
    std::string _fullMessage; ///< Formatted error with context
};

using SyntaxError     = ConfigParseError;
using UnexpectedToken = ConfigParseError;
using TokenizerError  = ConfigParseError;
using ValidationError = ConfigParseError;
