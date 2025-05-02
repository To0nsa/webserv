/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:37:06 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/02 21:13:41 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Server.hpp
 * @brief   Declares the Server class.
 *
 * @details This file defines the Server class, which models a single server block
 * as defined in the configuration file. It encapsulates configuration directives such as
 * listening port, host address, server name aliases, error pages, body size limits,
 * and associated Location blocks for routing.
 *
 * The class provides setters used during configuration parsing, and getters used
 * at runtime for request routing, virtual host selection, and error handling.
 *
 * @ingroup config
 */

#pragma once

#include "config/Location.hpp"
#include <map>
#include <string>
#include <vector>

/**
 * @brief Represents a configured HTTP server block.
 *
 * @details Holds all configuration parameters for a single server instance,
 * typically parsed from a configuration file. Encapsulates metadata such as
 * port, host IP, server name aliases, error pages, and location mappings.
 */
class Server {
  private:
    int                        port_;                 ///< Port number to bind to.
    std::string                host_;                 ///< Host IP address (e.g. 0.0.0.0).
    std::vector<std::string>   server_names_;         ///< Server name aliases.
    std::map<int, std::string> error_pages_;          ///< HTTP error code to file path.
    size_t                     client_max_body_size_; ///< Max request body size in bytes.
    std::vector<Location>      locations_;            ///< Location blocks (routes).

  public:
    // --- Constructor / Destructor ---

    Server();
    ~Server() = default;

    Server(const Server& other)            = default;
    Server& operator=(const Server& other) = default;

    // --- Setters ---

    void setPort(int port);
    void setHost(const std::string& host);
    void addServerName(const std::string& name);
    void setErrorPage(int code, const std::string& path);
    void setClientMaxBodySize(size_t size);
    void addLocation(const Location& location);

    // --- Getters ---

    int                               getPort() const noexcept;
    const std::string&                getHost() const noexcept;
    const std::vector<std::string>&   getServerNames() const noexcept;
    const std::map<int, std::string>& getErrorPages() const noexcept;
    size_t                            getClientMaxBodySize() const noexcept;
    const std::vector<Location>&      getLocations() const noexcept;

    /**
     * @brief Checks whether the given name matches one of this server's configured names.
     *
     * @details Used during virtual host resolution to determine if this server block
     * matches the value provided in the HTTP `Host:` header. This enables proper routing
     * when multiple server blocks are listening on the same port (name-based virtual hosting).
     *
     * @param name The value from the Host header (e.g. "example.com").
     * @return True if it matches any entry in server_names_.
     */
    bool hasServerName(const std::string& name) const;
};
