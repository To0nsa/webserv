/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PrintInfo.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 14:01:52 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/03 15:28:11 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include "config/Config.hpp"
#include "core/Location.hpp"
#include "core/Server.hpp"
#include <iostream>

void print_usage(void);
void print_config(Config& config);
