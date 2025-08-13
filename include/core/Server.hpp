/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:37:06 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/13 09:35:27 by nlouis           ###   ########.fr       */
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
 * @ingroup core
 */

#pragma once

#include "core/Location.hpp"
#include <map>
#include <string>
#include <vector>
#include <string_view>
#include <cstddef>

/**
 * @brief Represents a virtual server block.
 *
 * @details Encapsulates configuration for a single virtual host, including
 *          its listening address, server names, error pages, body size limits,
 *          and nested location blocks. Each Server instance corresponds to a
 *          `server` block in the configuration file and participates in request
 *          routing based on host and port matching.
 *
 * @ingroup core
 */
class Server {
  private:
    int                        _port;         ///< Port to listen on (0–65535).
    std::string                _host;         ///< IP address to bind (e.g., "0.0.0.0").
    std::vector<std::string>   _server_names; ///< List of server name aliases (Host-based routing).
    std::map<int, std::string> _error_pages;  ///< Maps HTTP error codes to custom error page paths.
    std::size_t                _client_max_body_size; ///< Maximum allowed body size per request (bytes).
    std::vector<Location> _locations;            ///< Set of location blocks defined for this server.

  public:
    ///////////////////
    // --- Constructor
    /**
     * @brief Constructs a Server instance with default settings.
     *
     * @details Initializes default values for host (`"0.0.0.0"`), port (`80`),
     *          client max body size (`1 MiB`), and empty location/error blocks.
     *          Intended to be populated via configuration parsing.
     *
     * @ingroup core
     */
    Server();
    ~Server()                              = default;
    Server(const Server& other)            = default;
    Server& operator=(const Server& other) = default;

    ///////////////
    // --- Setters
    /**
     * @brief Sets the port number this server will listen on.
     *
     * @details This port must be in the range [0, 65535]. It determines which TCP port
     *          the server binds to for accepting incoming connections. Typically set via
     *          the `listen` directive in the configuration file. Validation is done before calling.
     *
     * @param port The TCP port number to bind to.
     * @ingroup core
     */
    void setPort(int port) noexcept;
    
    /**
     * @brief Sets the IP address to bind this server to.
     *
     * @details The host determines which network interface(s) the server will listen on.
     *          A value of `"0.0.0.0"` binds to all interfaces. This is typically configured
     *          via the `host` directive in the configuration file. No validation is done here.
     *
     * @param host The IP address to bind (e.g., "127.0.0.1" or "0.0.0.0").
     * @ingroup core
     */
    void setHost(std::string_view host) noexcept;
    
    /**
     * @brief Adds a server name alias for this virtual host.
     *
     * @details Server names are used to match the `Host` header of incoming HTTP requests.
     *          Multiple names can be added to support name-based virtual hosting.
     *          This method appends without deduplication.
     *
     * @param name The server name to add (e.g., "example.com").
     * @ingroup core
     */
    void addServerName(std::string_view name);
    
    /**
     * @brief Sets a custom error page for a specific HTTP status code.
     *
     * @details Associates an HTTP error code (e.g., 404, 500) with a file path
     *          that will be served when that error occurs. Overrides the default
     *          error response. Multiple codes can share the same file path.
     *
     * @param code The HTTP error status code to override.
     * @param path The file path to serve as the custom error page.
     * @ingroup core
     */
    void setErrorPage(int code, const std::string& path);
    
    /**
     * @brief Sets the maximum allowed size for the HTTP request body.
     *
     * @details Used to limit the size of incoming requests, particularly for
     *          POST and PUT methods. If a request exceeds this size, the server
     *          should reject it with a 413 Payload Too Large response.
     *
     * @param size Maximum body size in bytes.
     * @ingroup core
     */
    void setClientMaxBodySize(std::size_t size) noexcept;
    
    /**
     * @brief Adds a location block to this server.
     *
     * @details Appends a new `Location` object representing a URI-matching block
     *          with its own configuration. Locations define routing rules and behavior
     *          for specific URI prefixes under this server.
     *
     * @param location The `Location` instance to add.
     * @ingroup core
     */
    void addLocation(const Location& location);

    ///////////////
    // --- Getters
    /**
      * @brief Returns the port number this server listens on.
      *
      * @details Reflects the `listen` directive from the configuration. Value is in
      *          the valid TCP range [0, 65535].
      *
      * @return The configured TCP port.
      * @ingroup core
      */
    int getPort() const noexcept;
    
    /**
     * @brief Returns the configured host IP address for this server.
     *
     * @details This address determines which local interface(s) the server binds to.
     *          A value of `"0.0.0.0"` means it will accept connections on all interfaces.
     *          Typically set via the `host` directive in the configuration file.
     *
     * @return Reference to the host IP address string.
     * @ingroup core
     */
    const std::string& getHost() const noexcept;
    
    /**
     * @brief Returns the list of server name aliases for this virtual host.
     *
     * @details These names are used to match the `Host` header in incoming HTTP requests.
     *          If none match, the first declared server for the host:port is used as default.
     *          Configured via the `server_name` directive.
     *
     * @return Reference to the list of server names.
     * @ingroup core
     */
    const std::vector<std::string>& getServerNames() const noexcept;

    /**
     * @brief Returns the default server name for this virtual host.
     *
     * @details If no server names have been configured for this instance, the method
     *          returns `"localhost"`. Otherwise, it returns the first declared name
     *          from the configured list of server names. This value is used as the
     *          fallback when no explicit name match is found during host-based
     *          request routing.
     *
     * @return The default server name string.
     * @ingroup core
     */
    const std::string               getDefaultServerName() const;
    
    /**
     * @brief Returns the mapping of HTTP error codes to custom error pages.
     *
     * @details This map associates specific HTTP status codes (e.g., 404, 500)
     *          with file paths to serve instead of default error messages.
     *          Configured via the `error_page` directive.
     *
     * @return Reference to the map of error codes to file paths.
     * @ingroup core
     */
    const std::map<int, std::string>& getErrorPages() const noexcept;
    /**
     * @brief Returns the maximum allowed size for the request body.
     *
     * @details This limit applies to the content length of incoming HTTP requests,
     *          including POST uploads. If exceeded, the server should return
     *          a 413 Payload Too Large error. Configured via the `client_max_body_size` directive.
     *
     * @return The maximum request body size in bytes.
     * @ingroup core
     */
    std::size_t getClientMaxBodySize() const noexcept;
    
    /**
     * @brief Returns the list of location blocks defined for this server.
     *
     * @details Each location block defines a URI prefix and associated behavior
     *          (e.g., root, methods, CGI, redirects). During request handling, the
     *          server selects the best-matching location based on the URI.
     *
     * @return Reference to the list of `Location` objects.
     * @ingroup core
     */
    const std::vector<Location>& getLocations() const noexcept;
    
    /**
     * @brief Returns a mutable reference to the server's location blocks.
     *
     * @details Allows in-place modification of the list of `Location` objects,
     *          typically used during configuration parsing to populate new routes.
     *          Use with care to avoid breaking routing logic.
     *
     * @return Reference to the list of `Location` objects.
     * @ingroup core
     */
    std::vector<Location>& getLocations() noexcept;
    
    /**
     * @brief Checks whether the server matches the given server name.
     *
     * @details Compares the provided name against the configured server names
     *          for this virtual host. Used during request routing based on the
     *          `Host` header in the HTTP request.
     *
     * @param name The server name to check (case-sensitive).
     * @return `true` if the name matches one of the configured server names.
     * @ingroup core
     */
    bool hasServerName(std::string_view name) const noexcept;
};
