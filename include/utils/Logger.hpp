/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Logger.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/25 12:09:58 by ktieu             #+#    #+#             */
/*   Updated: 2025/06/04 09:48:22 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <iostream>
#include <string>

enum class LogLevel { kDEBUG, INFO, WARN, ERROR };

class Logger {
  public:
    static void log(LogLevel level, const std::string& message);
    static void logFrom(LogLevel level, const std::string& from, const std::string& message);

  private:
    Logger()                               = delete; // Prevent instantiation
    ~Logger()                              = delete; // Prevent instantiation
    Logger(const Logger& org)              = delete; // Prevent copy
    Logger& operator=(const Logger& other) = delete; // Prevent assignment
};
