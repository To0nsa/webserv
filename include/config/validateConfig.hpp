/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   validateConfig.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 00:11:16 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 14:05:46 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    validateConfig.hpp
 * @brief   Declares validation for normalized configuration.
 *
 * @details Performs semantic checks after parsing/normalization to guarantee
 *          a runnable configuration (e.g., port/host collisions, invalid paths,
 *          duplicate server names on same listen, illegal method sets, etc.).
 *
 * @ingroup config_validating
 */

#pragma once

class Config;

void validateConfig(const Config& config);
