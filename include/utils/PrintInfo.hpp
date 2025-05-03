/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PrintInfo.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 14:01:52 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/03 15:03:43 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <iostream>
#include "config/Config.hpp"
#include "core/Server.hpp"
#include "config/Location.hpp"

void print_usage( void );
void print_config( Config& config );
