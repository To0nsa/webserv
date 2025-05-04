/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 10:34:50 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/03 15:33:16 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    config.hpp
 * @brief   Declares the Config class.
 *
 * @details Represents the parsed server configuration. Holds a list of `Server` instances
 * that define virtual hosts. Used by the HTTP router to select the correct server
 * based on port and Host header.
 * @ingroup config
 */

#pragma once

#include "core/Server.hpp"
#include <vector>

/**
 * @brief Top-level server configuration container.
 *
 * @details Holds one or more `Server` blocks parsed from the configuration file.
 * Each `Server` represents a virtual host, which may listen on different ports
 * and respond to different server names.
 * @ingroup config
 */
class Config {
  private:
    std::vector<Server> _servers; ///< List of all parsed servers

  public:
    // --- Constructor / Destructor ---
    Config()                         = default;
    ~Config()                        = default;
    Config(const Config&)            = default;
    Config& operator=(const Config&) = default;

    /**
     * @brief Enables efficient move construction.
     *
     * @details Transfers ownership of internal server list from another instance.
     * Improves performance when returning Config by value or storing it in containers.
     */
    Config(Config&&) noexcept = default;

    /**
     * @brief Enables efficient move assignment.
     *
     * @details Transfers ownership of internal server list from another instance.
     * Marked noexcept to support STL container performance and exception guarantees.
     */
    Config& operator=(Config&&) noexcept = default;

    // --- Public API ---
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
};
