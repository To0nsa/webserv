/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   stringUtils.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 20:09:44 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/15 23:00:37 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

int         parseInt(const std::string& value, const std::string& field, int line, int column,
                     const std::function<std::string()>& context_provider);
std::size_t parseByteSize(const std::string& value, const std::string& field, int line, int column,
                          const std::function<std::string()>& context_provider);
std::string toLower(const std::string&);
std::string toUpper(const std::string& s);
std::string formatBytes(std::size_t bytes);
std::string joinStrings(const std::vector<std::string>& list, const std::string& delim = ", ");
std::string trim(const std::string& str);
