#include "utils/Logger.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

// ANSI color codes
#define COLOR_RESET "\033[0m"
#define COLOR_DEBUG "\033[36m" // Cyan
#define COLOR_INFO "\033[32m"  // Green
#define COLOR_WARN "\033[33m"  // Yellow
#define COLOR_ERROR "\033[31m" // Red

std::string levelToString(LogLevel level) {
    switch (level) {
    case LogLevel::DEBUG:
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
    case LogLevel::DEBUG:
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
    std::ostream& os = std::cerr; // 👈 All logs to cerr
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
