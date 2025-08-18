/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   normalizeConfig.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 21:02:21 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 13:26:36 by nlouis           ###   ########.fr       */
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

/**
 * @brief Applies in-place normalization to a parsed configuration.
 *
 * @details Iterates over all servers/locations and fills defaults for:
 *          - client_max_body_size
 *          - error_page mappings
 *          - location root and index files
 *          - allowed HTTP methods
 *
 * @param config Parsed configuration to normalize (modified in place).
 *
 * @ingroup config_normalizing
 */
void normalizeConfig(Config& config);
