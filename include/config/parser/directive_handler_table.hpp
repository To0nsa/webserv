/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   directive_handler_table.hpp                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/08 17:10:31 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/08 17:14:05 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    directive_handler_table.hpp
 * @brief   Defines handler dispatch tables for server and location directives.
 *
 * @details This file contains the mappings of configuration directives (such as `listen`,
 *          `host`, `server_name`, etc.) to their respective handler functions. These handler
 *          functions are responsible for parsing and applying configuration settings to the
 *          corresponding `Server` or `Location` objects during the configuration file processing.
 *          The handler tables are used by the `ConfigParser` to delegate parsing tasks efficiently.
 *
 * @ingroup config
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
 * @typedef ServerHandler
 * @brief Type alias for the function signature handling server configuration directives.
 *
 * @details This type alias represents a function that takes a reference to a `Server` object,
 *          a list of arguments (`std::vector<std::string>`) parsed from a directive, the line
 *          and column numbers where the directive was found, and a `contextWindow` string for
 *          error reporting. The function is responsible for applying the parsed arguments to the
 *          `Server` configuration.
 *
 * @param s             The `Server` object to configure.
 * @param args          A vector of arguments parsed from the directive.
 * @param line          The line number where the directive was found.
 * @param column        The column number where the directive was found.
 * @param contextWindow A string containing additional context (e.g., surrounding lines in the
 * config).
 *
 * @ingroup config
 */
using ServerHandler = std::function<void(Server& s, const std::vector<std::string>& args, int line,
                                         int column, const std::string& contextWindow)>;

/**
 * @typedef LocationHandler
 * @brief Type alias for the function signature handling location configuration directives.
 *
 * @details This type alias represents a function that takes a reference to a `Location` object,
 *          a list of arguments (`std::vector<std::string>`) parsed from a directive, the line
 *          and column numbers where the directive was found, and a `contextWindow` string for
 *          error reporting. The function is responsible for applying the parsed arguments to the
 *          `Location` configuration.
 *
 * @param loc            The `Location` object to configure.
 * @param args           A vector of arguments parsed from the directive.
 * @param line           The line number where the directive was found.
 * @param column         The column number where the directive was found.
 * @param contextWindow  A string containing additional context (e.g., surrounding lines in the
 * config).
 *
 * @ingroup config
 */
using LocationHandler = std::function<void(Location& loc, const std::vector<std::string>& args,
                                           int line, int column, const std::string& contextWindow)>;

/**
 * @brief Returns the map of server directive handlers.
 *
 * @details This function returns a reference to a constant unordered map that maps
 *          server configuration directive names (as strings) to their corresponding
 *          handler functions (`ServerHandler`). The handler functions are responsible
 *          for parsing the directive arguments and applying them to the `Server` object.
 *          This map is used during configuration file parsing to invoke the correct
 *          handler for each server-related directive.
 *
 * @return A reference to the map of server directive handlers.
 * @ingroup config
 */
const std::unordered_map<std::string, ServerHandler>& serverHandlers();

/**
 * @brief Returns the map of location directive handlers.
 *
 * @details This function returns a reference to a constant unordered map that maps
 *          location configuration directive names (as strings) to their corresponding
 *          handler functions (`LocationHandler`). The handler functions are responsible
 *          for parsing the directive arguments and applying them to the `Location` object.
 *          This map is used during configuration file parsing to invoke the correct
 *          handler for each location-related directive.
 *
 * @return A reference to the map of location directive handlers.
 * @ingroup config
 */
const std::unordered_map<std::string, LocationHandler>& locationHandlers();

} // namespace directive