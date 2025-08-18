/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParseError.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/06 13:13:44 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 12:01:14 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    ConfigParseError.hpp
 * @brief   Declares parse/validation error types for the config subsystem.
 *
 * @details Provides a single exception class used for tokenizer, parser, and
 *          validator errors. The class formats a human-friendly message and
 *          optionally appends a contextual source line for diagnostics.
 *
 * @ingroup config_parse_error
 */

#pragma once

#include <exception>
#include <string>

/**
 * @brief Base exception for configuration processing errors.
 *
 * @details Carries a formatted message and an optional context line (e.g.,
 *          the offending source line). All config-related errors (tokenizer,
 *          parser, unexpected token, validation) alias this class.
 *
 * @ingroup config_parse_error
 */
class ConfigParseError : public std::exception {
  public:
    /** @name Construction & interface */
    ///@{
    ConfigParseError(const std::string& message, const std::string& context = "");
    const char* what() const noexcept override;
    ///@}

  protected:
    std::string _context;     ///< Optional contextual source line shown after the message.
    std::string _fullMessage; ///< Final formatted message (message + context arrow).
};

/** @brief Alias for syntax errors (tokenizer/parser). @ingroup config_parse_error */
using SyntaxError = ConfigParseError;
/** @brief Alias for unexpected token errors.        @ingroup config_parse_error */
using UnexpectedToken = ConfigParseError;
/** @brief Alias for tokenizer-specific errors.      @ingroup config_parse_error */
using TokenizerError = ConfigParseError;
/** @brief Alias for configuration validation errors.@ingroup config_parse_error */
using ValidationError = ConfigParseError;
