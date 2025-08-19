/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   directive_handler_table.hpp                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/08 17:10:31 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 12:36:11 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    directive_handler_table.hpp
 * @brief   Declares handler maps for server/location directives.
 *
 * @details Provides the callable signatures and lookup tables used by
 *          @ref ConfigParser to apply parsed directives to @ref Server
 *          and @ref Location instances. Each entry maps a directive
 *          keyword (e.g., "listen", "root") to a function that mutates
 *          the target object or throws on invalid arguments.
 *
 * @ingroup config_parsing
 */

#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Location.hpp"
#include "core/Server.hpp"

namespace directive {

/**
 * @brief Handler signature for server-level directives.
 *
 * @param s     Target server to mutate.
 * @param args  Directive arguments as strings (already token-collected).
 * @param line  Source line for diagnostics.
 * @param column Source column for diagnostics.
 * @param ctx   Contextual source snippet to append in error messages.
 *
 * @ingroup config_parsing
 */
using ServerHandler = std::function<void(Server& s, const std::vector<std::string>& args, int line,
                                         int column, const std::string& ctx)>;

/**
 * @brief Handler signature for location-level directives.
 *
 * @param loc   Target location to mutate.
 * @param args  Directive arguments as strings (already token-collected).
 * @param line  Source line for diagnostics.
 * @param column Source column for diagnostics.
 * @param ctx   Contextual source snippet to append in error messages.
 *
 * @ingroup config_parsing
 */
using LocationHandler = std::function<void(Location& loc, const std::vector<std::string>& args,
                                           int line, int column, const std::string& ctx)>;

const std::unordered_map<std::string, ServerHandler>&   serverHandlers();
const std::unordered_map<std::string, LocationHandler>& locationHandlers();

} // namespace directive
