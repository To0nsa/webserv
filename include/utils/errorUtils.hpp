/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   errorUtils.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/06 19:59:07 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/06 19:59:14 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file errorUtils.hpp
 * @brief Error formatting utilities.
 *
 * @details
 * Provides helper functions to standardize how error messages are formatted,
 * especially with line and column information, to improve diagnostics.
 */

/**
 * @defgroup ErrorUtils Error message formatting
 * @brief Helpers for formatting human-readable error messages.
 *
 * @details
 * These utilities generate consistent and contextual error messages for use
 * in exceptions and log output.
 * @{
 */

#pragma once

#include <string>

/**
 * @brief Formats an error message with line and column information.
 * @ingroup ErrorUtils
 *
 * @details
 * Constructs a human-readable error message of the form:
 * `"Line X, column Y: <msg>"`. Intended for consistent formatting across
 * parser or tokenizer errors.
 *
 * @param msg     The error message.
 * @param line    The line number where the error occurred.
 * @param column  The column number of the problematic element.
 *
 * @return A formatted string with line/column context.
 */
std::string formatError(const std::string& msg, int line, int column);

/** @} */ // end of ErrorUtils
