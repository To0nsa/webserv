/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Location.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/02 13:45:05 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/03 15:35:35 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Location.hpp
 * @brief   Defines the Location class used for per-path server configuration.
 *
 * @details Each Location object holds the configuration for a specific URL path
 * within a virtual server, including allowed methods, root, autoindex setting,
 * and CGI behavior.
 * @ingroup core
 */

#pragma once

#include <set>
#include <string>

/**
 * @defgroup config Server Configuration
 * @brief Classes and data structures used to represent configuration blocks.
 * @{
 */

/**
 * @brief Represents a location block in the server configuration.
 *
 * @details Encapsulates path-specific configuration such as allowed HTTP methods,
 * root directory, CGI extension, autoindex behavior, redirection settings,
 * and upload storage path.
 * @ingroup core
 */
class Location {
  public:
    Location();
    ~Location()                                = default;
    Location(const Location& other)            = default;
    Location& operator=(const Location& other) = default;

    // --- Setters ---

    void setPath(const std::string& path);
    void addMethod(const std::string& method);
    void setRoot(const std::string& root);
    void setIndex(const std::string& index);
    void setAutoindex(bool enabled);
    void setRedirect(const std::string& target, int code = 301);
    void setUploadStore(const std::string& path);
    void setCgiExtension(const std::string& ext);

    // --- Getters ---

    const std::string&           getPath() const;
    const std::set<std::string>& getMethods() const;
    const std::string&           getRoot() const;
    const std::string&           getIndex() const;
    bool                         isAutoindexEnabled() const;
    bool                         hasRedirect() const;
    const std::string&           getRedirect() const;
    int                          getReturnCode() const;
    const std::string&           getUploadStore() const;
    const std::string&           getCgiExtension() const;

    // --- Logic helpers ---

    /**
     * @brief Verifies if the given HTTP method is allowed for this location.
     *
     * @details Checks whether the method exists in the set of allowed methods
     * configured for this location (e.g., "GET", "POST", "DELETE").
     *
     * @param method The HTTP method string to check.
     * @return True if the method is allowed, false otherwise.
     */
    bool isMethodAllowed(const std::string& method) const;

    /**
     * @brief Checks if this location matches a given request URI.
     *
     * @details This is typically a prefix match. For example, if the path is "/api",
     * then this method returns true for "/api/users" and "/api/status".
     *
     * @param uri The full request URI to match.
     * @return True if the location path is a prefix of the URI.
     */
    bool matchesPath(const std::string& uri) const;

    /**
     * @brief Resolves a request URI into a full filesystem path.
     *
     * @details Joins the root directory and the URI suffix after the location path.
     * Returns an empty string if the URI does not match this location.
     *
     * @param uri The full request URI.
     * @return The absolute path to the requested resource.
     */
    std::string resolveAbsolutePath(const std::string& uri) const;

    /**
     * @brief Indicates whether file uploads are allowed for this location.
     *
     * @return True if an upload directory is configured, false otherwise.
     */
    bool isUploadEnabled() const;

    /**
     * @brief Determines if the URI targets a CGI script.
     *
     * @param uri The requested URI.
     * @return True if the URI ends with the configured CGI extension.
     */
    bool isCgiRequest(const std::string& uri) const;

    /**
     * @brief Returns the full path to the index file, if one is set.
     *
     * @return The absolute path to the index file or an empty string.
     */
    std::string getEffectiveIndexPath() const;

  private:
    std::string           _path;          ///< The URL path this location matches.
    std::set<std::string> _methods;       ///< Set of allowed HTTP methods.
    std::string           _root;          ///< Root directory for file serving.
    std::string           _index;         ///< Default index file name.
    bool                  _autoindex;     ///< Whether to enable directory listing.
    std::string           _redirect;      ///< Redirection target URL.
    int                   _return_code;   ///< HTTP status code for redirection.
    std::string           _upload_store;  ///< Directory for uploaded files.
    std::string           _cgi_extension; ///< CGI file extension (e.g., ".php").
};

/** @} */
