/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   errorUtils.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 21:08:50 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/06 20:14:01 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    errorUtils.cpp
 * @brief   Implements error formatting utilities.
 *
 * @details
 * Provides the implementation of `formatError`, a helper function that
 * generates consistent diagnostic messages with line and column info.
 * @ingroup ErrorUtils
 */

#include "utils/errorUtils.hpp"

std::string formatError(const std::string& msg, int line, int column) {
    // Builds a standardized error message with line and column context
    return "Line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + msg;
}