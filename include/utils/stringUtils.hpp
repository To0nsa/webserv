/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   stringUtils.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 20:09:44 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/07 11:56:52 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file stringUtils.hpp
 * @brief Utility functions for parsing numeric string values.
 *
 * @details
 * Provides functions to parse integers and human-readable byte sizes from strings.
 * Supports contextual error reporting for use in configuration parsing or diagnostics.
 */

/**
 * @defgroup StringUtils String parsing utilities
 * @brief Helpers to parse integers and memory sizes from strings.
 *
 * @details
 * This group provides low-level utilities used in configuration parsing for converting
 * string representations of numbers and byte sizes into typed values with context-aware
 * error handling.
 * @{
 */

#pragma once

#include <cstddef>
#include <functional>
#include <string>

/**
 * @brief Parses an integer from a string.
 * @ingroup StringUtils
 *
 * @details
 * Parses a non-negative integer from a string. Throws `std::invalid_argument` on failure.
 * Context is omitted; this version is intended for simple cases.
 *
 * @param value The string to parse.
 * @return The parsed integer.
 */
int parseInt(const std::string& value);

/**
 * @brief Parses a byte size from a human-readable string (e.g., "512K", "1M").
 * @ingroup StringUtils
 *
 * @details
 * Supports optional suffixes: K (kilobytes), M (megabytes), G (gigabytes).
 * Throws `std::invalid_argument` on failure. Context is omitted.
 *
 * @param value The string to parse.
 * @return The parsed size in bytes.
 */
std::size_t parseByteSize(const std::string& value);

/**
 * @brief Parses an integer with detailed error context.
 * @ingroup StringUtils
 *
 * @details
 * Parses a non-negative integer and throws a detailed `std::invalid_argument` error
 * on failure. Includes the field name, line/column, and optional context string.
 *
 * @param value             The string to parse.
 * @param field             The name of the field being parsed.
 * @param line              Line number where the value is found.
 * @param column            Column number where the value starts.
 * @param context_provider  A lambda that returns extra context (e.g., the source line).
 *
 * @return The parsed integer.
 */
int parseInt(const std::string& value, const std::string& field, int line, int column,
             const std::function<std::string()>& context_provider);

/**
 * @brief Parses a byte size with detailed error context.
 * @ingroup StringUtils
 *
 * @details
 * Accepts size strings with suffixes (K/M/G). On failure, throws `std::invalid_argument`
 * with details including field name, line/column, and optional context string.
 *
 * @param value             The string to parse.
 * @param field             The name of the field being parsed.
 * @param line              Line number where the value is found.
 * @param column            Column number where the value starts.
 * @param context_provider  A lambda that returns extra context (e.g., the source line).
 *
 * @return The parsed size in bytes.
 */
std::size_t parseByteSize(const std::string& value, const std::string& field, int line, int column,
                          const std::function<std::string()>& context_provider);
/**
 * @brief Converts a string to lowercase.
 *
 * @details Returns a copy of the input string with all ASCII alphabetic characters
 *          converted to lowercase using the current locale rules.
 *
 * @param str The input string to convert.
 * @return A lowercase copy of the input string.
 */
std::string toLower(const std::string&);

/** @} */ // end of StringUtils
