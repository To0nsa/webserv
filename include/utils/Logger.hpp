/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Logger.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/25 12:09:58 by ktieu             #+#    #+#             */
/*   Updated: 2025/08/17 12:30:59 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <string> // for string

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
