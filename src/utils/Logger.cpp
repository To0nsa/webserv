/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Logger.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/15 22:54:33 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 19:55:37 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Logger.cpp
 * @brief   Simple color-coded logging utility.
 *
 * @details Implements the @ref Logger class methods and related helper
 *          functions for mapping log levels to human-readable strings
 *          and ANSI terminal colors. Supports standard log levels
 *          (`DEBUG`, `INFO`, `WARN`, `ERROR`) and can optionally
 *          prefix messages with a source identifier.
 *
 *          Output is written to `stdout` for non-error messages and
 *          `stderr` for `ERROR` level messages, ensuring separation
 *          of normal and error output streams. ANSI escape codes are
 *          used for color, which may not render correctly in all
 *          terminals.
 *
 * @ingroup utils
 *
 * @note This logger is intended for human-readable output during
 *       development and runtime diagnostics, not structured logging.
 *       For production environments or log parsing, consider extending
 *       it to support formats like JSON or syslog.
 */

#include "utils/Logger.hpp"

#include <iostream> // for basic_ostream, operator<<, endl, cerr, cout

// ANSI color codes
#define COLOR_RESET "\033[0m"
#define COLOR_DEBUG "\033[36m" // Cyan
#define COLOR_INFO "\033[32m"  // Green
#define COLOR_WARN "\033[33m"  // Yellow
#define COLOR_ERROR "\033[31m" // Red

std::string levelToString(LogLevel level) {
    switch (level) {
    case LogLevel::kDEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARN:
        return "WARN";
    case LogLevel::ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

const char* levelColor(LogLevel level) {
    switch (level) {
    case LogLevel::kDEBUG:
        return COLOR_DEBUG;
    case LogLevel::INFO:
        return COLOR_INFO;
    case LogLevel::WARN:
        return COLOR_WARN;
    case LogLevel::ERROR:
        return COLOR_ERROR;
    default:
        return COLOR_RESET;
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level == LogLevel::ERROR) {
        std::cerr << levelColor(level) << "[" << levelToString(level) << "]" << COLOR_RESET << " "
                  << message << std::endl;
        return;
    }
    std::cout << levelColor(level) << "[" << levelToString(level) << "]" << COLOR_RESET << " "
              << message << std::endl;
}

void Logger::logFrom(LogLevel level, const std::string& from, const std::string& message) {
    if (from.empty()) {
        log(level, message);
        return;
    }
    if (level == LogLevel::ERROR) {
        std::cerr << levelColor(level) << "[" << levelToString(level) << "] " << from << " : "
                  << COLOR_RESET << " " << message << std::endl;
        return;
    }
    std::cout << levelColor(level) << "[" << levelToString(level) << "] " << from << " : "
              << COLOR_RESET << " " << message << std::endl;
}
