/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   normalizeConfig.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 21:02:21 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 15:58:50 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    normalizeConfig.hpp
 * @brief   Declares normalization utilities for parsed configuration.
 *
 * @details Applies canonical defaults and fills in missing values after parsing,
 *          so downstream components can rely on predictable, complete settings.
 *          Typical defaults include body size limits, error pages, roots, indices,
 *          and allowed methods.
 *
 * @ingroup config_normalizing
 */

#pragma once

#include "config/Config.hpp"

void normalizeConfig(Config& config);
