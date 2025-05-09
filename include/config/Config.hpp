/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 10:34:50 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/08 15:57:19 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Config.hpp
 * @brief   Declares the Config class, a container for server configurations.
 *
 * @details The Config class aggregates Server instances parsed from a
 *          configuration file, representing virtual hosts. It provides
 *          methods to add new Server objects and retrieve the collection
 *          for HTTP request routing based on host and port matching.
 *
 * @ingroup config
 */

#pragma once

#include "core/Server.hpp"
#include <vector>

/**
 * @brief   Top-level server configuration container.
 *
 * @details Manages a collection of Server objects, each corresponding to
 *          a virtual host definition in the configuration file. Servers
 *          may listen on different ports, respond to specific hostnames,
 *          and define custom error pages and location blocks.
 *
 * @ingroup config
 */
class Config {
  public:
    ////////////////////////////////
    // --- Constructor
    Config()                             = default;
    ~Config()                            = default;
    Config(const Config&)                = default;
    Config& operator=(const Config&)     = default;
    Config(Config&&) noexcept            = default;
    Config& operator=(Config&&) noexcept = default;

    //////////////////
    // --- Public API
    /**
     * @brief Adds a new Server configuration.
     *
     * @param server A fully initialized Server object.
     */
    void addServer(const Server& server);
    /**
     * @brief Returns the list of configured servers.
     *
     * @return Read-only reference to internal server list.
     */
    const std::vector<Server>& getServers() const;
    /**
     * @brief Provides mutable access to the configured servers.
     *
     * @details Returns a non-const reference to the internal server list,
     *          allowing modifications such as adding, removing, or reordering Servers.
     *
     * @return Reference to the vector of Server objects stored internally.
     */
    std::vector<Server>& getServers();

  private:
    std::vector<Server> _servers; ///< List of all parsed servers
};
