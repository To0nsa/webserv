/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   errorUtils.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/06 19:59:07 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/15 22:59:56 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <string>

std::string formatError(const std::string& msg, int line, int column);
