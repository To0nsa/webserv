/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Logger.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/25 12:09:58 by ktieu             #+#    #+#             */
/*   Updated: 2025/08/18 19:48:00 by nlouis           ###   ########.fr       */
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
    Logger()                               = delete;
    ~Logger()                              = delete;
    Logger(const Logger& org)              = delete;
    Logger& operator=(const Logger& other) = delete;
};
