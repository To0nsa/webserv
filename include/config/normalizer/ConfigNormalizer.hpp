/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigNormalizer.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 21:02:21 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/12 21:02:54 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    ConfigNormalizer.hpp
 * @brief   Declares normalization logic for incomplete or partial server configurations.
 *
 * @details Provides the `normalizeServer` function used during the config parsing pipeline
 * to ensure each `Server` and its associated `Location` blocks have all mandatory
 * fields populated with sane defaults. This includes defaults for root directories,
 * index files, allowed HTTP methods, error pages, and client body size.
 *
 * @ingroup config
 */

#pragma once
#include "config/Config.hpp"

/**
 * @brief Normalizes a server block by applying default values.
 *
 * @details This function ensures that every `Server` object has consistent and complete
 * configuration by setting default values for critical fields when missing.
 * It applies default error pages, a default client body size if unset,
 * and ensures every `Location` has valid root, index, and allowed methods.
 *
 * @param server The server block to normalize. Modified in place.
 *
 * @ingroup config
 */
void normalizeServer(Server& server);
