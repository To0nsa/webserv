/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   printInfo.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 14:01:52 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/17 12:32:28 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <string> // for string
class Config;

std::string printUsage(void);
void        printConfig(Config& config);
