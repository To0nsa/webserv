/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:37:06 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/14 14:31:55 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Server.hpp
 * @brief   Declares the Server class for virtual host configuration.
 *
 * @details Represents a virtual server block parsed from the configuration file.
 *          Each Server instance can listen on a specific host:port pair, define
 *          error pages, configure body size limits, and contain multiple location
 *          blocks with their own routing rules and behavior.
 *
 * @ingroup server
 */

#pragma once

#include "core/Location.hpp"

#include <cstddef>     // std::size_t for body size type
#include <map>         // std::map for _error_pages (HTTP code → error page path)
#include <string>      // std::string for host, server names, error page paths
#include <string_view> // std::string_view for lightweight parameter passing
#include <vector>      // std::vector for _server_names and _locations

/**
 * @brief Represents a virtual server block.
 *
 * @details Encapsulates configuration for a single virtual host, including
 *          its listening address, server names, error pages, body size limits,
 *          and nested location blocks. Each Server instance corresponds to a
 *          `server` block in the configuration file and participates in request
 *          routing based on host and port matching.
 *
 * @ingroup server
 */
class Server {
  private:
    //=== Data ================================================================

    int                        _port;                 ///< Port to listen on (0–65535).
    std::string                _host;                 ///< Bind address (e.g., "0.0.0.0").
    std::vector<std::string>   _server_names;         ///< Host-based routing aliases.
    std::map<int, std::string> _error_pages;          ///< HTTP code → error page path.
    std::size_t                _client_max_body_size; ///< Max request body size (bytes).
    std::vector<Location>      _locations;            ///< Location blocks for this server.

  public:
    //=== Construction & Special Members =====================================

    /** @name Construction & special members */
    ///@{
    Server();
    ~Server()                              = default;
    Server(const Server& other)            = default;
    Server& operator=(const Server& other) = default;
    ///@}

    //=== Configuration Setters ==============================================

    /** @name Configuration setters */
    ///@{
    void setPort(int port) noexcept;
    void setHost(std::string_view host) noexcept;
    void addServerName(std::string_view name);
    void setErrorPage(int code, const std::string& path);
    void setClientMaxBodySize(std::size_t size) noexcept;
    void addLocation(const Location& location);
    ///@}

    //=== Queries (Getters) ===================================================

    /** @name Queries (getters) */
    ///@{
    int                               getPort() const noexcept;
    const std::string&                getHost() const noexcept;
    const std::vector<std::string>&   getServerNames() const noexcept;
    const std::string                 getDefaultServerName() const;
    const std::map<int, std::string>& getErrorPages() const noexcept;
    std::size_t                       getClientMaxBodySize() const noexcept;
    const std::vector<Location>&      getLocations() const noexcept;
    std::vector<Location>&            getLocations() noexcept;
    ///@}

    //=== Matching & Predicates ==============================================

    /** @name Matching & predicates */
    ///@{
    bool hasServerName(std::string_view name) const noexcept;
    ///@}
};
