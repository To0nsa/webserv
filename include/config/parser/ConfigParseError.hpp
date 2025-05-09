/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParseError.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/06 13:13:44 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/08 19:43:01 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    ConfigParseError.hpp
 * @brief   Defines error classes for configuration parsing.
 *
 * @details This file declares the `ConfigParseError` class, which is used for handling
 *          syntax errors encountered during the parsing of configuration files. The class
 *          extends `std::invalid_argument` to provide additional context such as the
 *          specific location (line, column) and surrounding context (window) of the error.
 *          This is crucial for debugging and reporting configuration issues effectively.
 *
 * @ingroup config
 */

#pragma once

#include <stdexcept>
#include <string>

/**
 * @class ConfigParseError
 * @brief Exception class for handling configuration parse errors.
 *
 * @details This class extends `std::invalid_argument` to represent errors that occur
 *          during the parsing of configuration files. It includes additional context,
 *          such as the specific line and column where the error was detected, as well as
 *          the surrounding context (context window) for better debugging.
 *          The exception can be thrown whenever an invalid or unexpected configuration
 *          directive or argument is encountered.
 *
 * @ingroup config
 */
class ConfigParseError : public std::invalid_argument {
  public:
    /**
     * @brief Constructs a ConfigParseError with a detailed message and context.
     *
     * @details This constructor initializes the `ConfigParseError` exception with a
     *          custom error message and an optional context string. The context provides
     *          additional information about the surrounding configuration, which is useful
     *          for debugging the specific error in the config file.
     *          If no context is provided, an empty string is used.
     *
     * @param message The error message describing the issue encountered during parsing.
     * @param context The context window containing the surrounding lines or relevant
     * 				  information from the configuration file.
     *                Defaults to an empty string if no context is provided.
     */
    explicit ConfigParseError(const std::string& message, std::string context = "");
    virtual ~ConfigParseError()                              = default;
    ConfigParseError(const ConfigParseError&)                = default;
    ConfigParseError& operator=(const ConfigParseError&)     = default;
    ConfigParseError(ConfigParseError&&) noexcept            = default;
    ConfigParseError& operator=(ConfigParseError&&) noexcept = default;

  protected:
    std::string context_; ///< The context window containing lines around the error.
};

/**
 * @typedef SyntaxError
 * @brief Alias for ConfigParseError representing syntax-level issues.
 */
using SyntaxError = ConfigParseError;

/**
 * @typedef UnexpectedToken
 * @brief Alias for ConfigParseError representing unexpected tokens.
 */
using UnexpectedToken = ConfigParseError;

/**
 * @typedef TokenizerError
 * @brief Alias for ConfigParseError representing tokenization failures.
 */
using TokenizerError = ConfigParseError;
