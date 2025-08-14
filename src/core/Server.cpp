/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:51:19 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/14 14:34:04 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Server.cpp
 * @brief   Implements the Server class used for virtual host configuration.
 *
 * @details This file defines all methods of the Server class, including setters,
 *          getters, and helper logic for managing host binding, server names,
 *          error pages, body size limits, and associated location blocks.
 *          It is part of the configuration system and supports parsing and runtime use.
 *
 * @ingroup server
 */

#include "core/Server.hpp"       // Server class declaration
#include "utils/stringUtils.hpp" // toLower() for case‐insensitive server names

#include <algorithm>   // std::any_of
#include <cstddef>     // std::size_t (body size)
#include <map>         // std::map (used in setErrorPage, getErrorPages)
#include <string>      // std::string (host, names, paths)
#include <string_view> // std::string_view parameters
#include <vector>      // std::vector (server names, locations)

//=== Construction & Special Members =====================================
/**
 * @brief Constructs a Server instance with default settings.
 *
 * @details Initializes default values for host (`"0.0.0.0"`), port (`80`),
 *          client max body size (`1 MiB`), and empty location/error blocks.
 *          Intended to be populated via configuration parsing.
 *
 * @ingroup server
 */
Server::Server()
    : _port(80),                             // Default HTTP port
      _host("0.0.0.0"),                      // Default bind address
      _client_max_body_size(1 * 1024 * 1024) // 1 MiB
{
}

//=== Configuration Setters ==============================================
/**
 * @brief Sets the port number this server will listen on.
 *
 * @details This port must be in the range [0, 65535]. It determines which TCP port
 *          the server binds to for accepting incoming connections. Typically set via
 *          the `listen` directive in the configuration file. Validation is done before calling.
 *
 * @param port The TCP port number to bind to.
 * @ingroup server
 */
void Server::setPort(int port) noexcept {
    _port = port;
}

/**
 * @brief Sets the IP address to bind this server to.
 *
 * @details The host determines which network interface(s) the server will listen on.
 *          A value of `"0.0.0.0"` binds to all interfaces. This is typically configured
 *          via the `host` directive in the configuration file. No validation is done here.
 *
 * @param host The IP address to bind (e.g., "127.0.0.1" or "0.0.0.0").
 * @ingroup server
 */
void Server::setHost(std::string_view host) noexcept {
    _host = host;
}

/**
 * @brief Adds a server name alias for this virtual host.
 *
 * @details Server names are used to match the `Host` header of incoming HTTP requests.
 *          Multiple names can be added to support name-based virtual hosting.
 *          This method appends without deduplication.
 *
 * @param name The server name to add (e.g., "example.com").
 * @ingroup server
 */
void Server::addServerName(std::string_view name) {
    // Append the given server name to the list of aliases.
    _server_names.emplace_back(toLower(std::string(name)));
}

/**
 * @brief Sets a custom error page for a specific HTTP status code.
 *
 * @details Associates an HTTP error code (e.g., 404, 500) with a file path
 *          that will be served when that error occurs. Overrides the default
 *          error response. Multiple codes can share the same file path.
 *
 * @param code The HTTP error status code to override.
 * @param path The file path to serve as the custom error page.
 * @ingroup server
 */
void Server::setErrorPage(int code, const std::string& path) {
    // Map the given HTTP status code to a custom error page path.
    _error_pages[code] = path;
}

/**
 * @brief Sets the maximum allowed size for the HTTP request body.
 *
 * @details Used to limit the size of incoming requests, particularly for
 *          POST and PUT methods. If a request exceeds this size, the server
 *          should reject it with a 413 Payload Too Large response.
 *
 * @param size Maximum body size in bytes.
 * @ingroup server
 */
void Server::setClientMaxBodySize(std::size_t size) noexcept {
    _client_max_body_size = size;
}

/**
 * @brief Adds a location block to this server.
 *
 * @details Appends a new `Location` object representing a URI-matching block
 *          with its own configuration. Locations define routing rules and behavior
 *          for specific URI prefixes under this server.
 *
 * @param location The `Location` instance to add.
 * @ingroup server
 */
void Server::addLocation(const Location& location) {
    // Add a new location block to the server's routing table.
    _locations.push_back(location);
}

//=== Queries (Getters) ===================================================

/**
 * @brief Returns the port number this server listens on.
 *
 * @details Reflects the `listen` directive from the configuration. Value is in
 *          the valid TCP range [0, 65535].
 *
 * @return The configured TCP port.
 * @ingroup server
 */
int Server::getPort() const noexcept {
    return _port;
}

/**
 * @brief Returns the configured host IP address for this server.
 *
 * @details This address determines which local interface(s) the server binds to.
 *          A value of `"0.0.0.0"` means it will accept connections on all interfaces.
 *          Typically set via the `host` directive in the configuration file.
 *
 * @return Reference to the host IP address string.
 * @ingroup server
 */
const std::string& Server::getHost() const noexcept {
    return _host;
}

/**
 * @brief Returns the list of server name aliases for this virtual host.
 *
 * @details These names are used to match the `Host` header in incoming HTTP requests.
 *          If none match, the first declared server for the host:port is used as default.
 *          Configured via the `server_name` directive.
 *
 * @return Reference to the list of server names.
 * @ingroup server
 */
const std::vector<std::string>& Server::getServerNames() const noexcept {
    return _server_names;
}

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
 * @ingroup server
 */
const std::string Server::getDefaultServerName() const {
    return _server_names.empty() ? "localhost" : _server_names.front();
}

/**
 * @brief Returns the mapping of HTTP error codes to custom error pages.
 *
 * @details This map associates specific HTTP status codes (e.g., 404, 500)
 *          with file paths to serve instead of default error messages.
 *          Configured via the `error_page` directive.
 *
 * @return Reference to the map of error codes to file paths.
 * @ingroup server
 */
const std::map<int, std::string>& Server::getErrorPages() const noexcept {
    return _error_pages;
}

/**
 * @brief Returns the maximum allowed size for the request body.
 *
 * @details This limit applies to the content length of incoming HTTP requests,
 *          including POST uploads. If exceeded, the server should return
 *          a 413 Payload Too Large error. Configured via the `client_max_body_size` directive.
 *
 * @return The maximum request body size in bytes.
 * @ingroup server
 */
std::size_t Server::getClientMaxBodySize() const noexcept {
    return _client_max_body_size;
}

/**
 * @brief Returns the list of location blocks defined for this server.
 *
 * @details Each location block defines a URI prefix and associated behavior
 *          (e.g., root, methods, CGI, redirects). During request handling, the
 *          server selects the best-matching location based on the URI.
 *
 * @return Reference to the list of `Location` objects.
 * @ingroup server
 */
const std::vector<Location>& Server::getLocations() const noexcept {
    return _locations;
}

/**
 * @brief Returns a mutable reference to the server's location blocks.
 *
 * @details Allows in-place modification of the list of `Location` objects,
 *          typically used during configuration parsing to populate new routes.
 *          Use with care to avoid breaking routing logic.
 *
 * @return Reference to the list of `Location` objects.
 * @ingroup server
 */
std::vector<Location>& Server::getLocations() noexcept {
    return _locations;
}

//=== Matching & Predicates ==============================================
/**
 * @brief Checks whether the server matches the given server name.
 *
 * @details Compares the provided name against the configured server names
 *          for this virtual host. Used during request routing based on the
 *          `Host` header in the HTTP request.
 *
 * @param name The server name to check (case-sensitive).
 * @return `true` if the name matches one of the configured server names.
 * @ingroup server
 */
bool Server::hasServerName(std::string_view name) const noexcept {
    // Check if the given name matches any of the configured server names.
    return std::any_of(_server_names.begin(), _server_names.end(),
                       [&name](const std::string& s) { return s == name; });
}
