/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Location.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/02 13:45:05 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/02 13:55:34 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Location.hpp
 * @brief   Defines the Location class used for per-path server configuration.
 *
 * @details Each Location object holds the configuration for a specific URL path
 * within a virtual server, including allowed methods, root, autoindex setting,
 * and CGI behavior.
 * @ingroup config
 */

#pragma once

#include <string>
#include <set>

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
 * @ingroup config
 */
class Location {
public:
    /**
     * @brief Constructs a Location object with default values.
     */
    Location();

    /**
     * @brief Default destructor.
     */
    ~Location() = default;

    /**
     * @brief Default copy constructor.
     */
    Location(const Location& other) = default;

    /**
     * @brief Default copy assignment operator.
     */
    Location& operator=(const Location& other) = default;

    // --- Setters ---

    /**
     * @brief Sets the path associated with this location.
     *
     * @param path The URL path (e.g., "/images").
     */
    void setPath(const std::string& path);

    /**
     * @brief Adds an allowed HTTP method for this location.
     *
     * @param method The HTTP method to allow (e.g., "GET", "POST").
     */
    void addMethod(const std::string& method);

    /**
     * @brief Sets the root directory for this location.
     *
     * @param root The filesystem path to serve files from.
     */
    void setRoot(const std::string& root);

    /**
     * @brief Sets the default index file for this location.
     *
     * @param index The filename to serve when a directory is requested.
     */
    void setIndex(const std::string& index);

    /**
     * @brief Enables or disables autoindexing.
     *
     * @param enabled True to enable directory listing.
     */
    void setAutoindex(bool enabled);

    /**
     * @brief Sets the HTTP redirect target and status code.
     *
     * @param target The redirection target URL.
     * @param code The HTTP status code to return (default is 301).
     */
    void setRedirect(const std::string& target, int code = 301);

    /**
     * @brief Sets the upload directory for POST requests.
     *
     * @param path Directory path where uploaded files will be stored.
     */
    void setUploadStore(const std::string& path);

    /**
     * @brief Sets the file extension used to trigger CGI execution.
     *
     * @param ext The CGI file extension (e.g., ".php", ".py").
     */
    void setCgiExtension(const std::string& ext);

    // --- Getters ---

    /**
     * @brief Returns the configured path.
     *
     * @return The URL path.
     */
    const std::string& getPath() const;

    /**
     * @brief Returns the set of allowed HTTP methods.
     *
     * @return A set of strings representing allowed methods.
     */
    const std::set<std::string>& getMethods() const;

    /**
     * @brief Returns the root directory for file serving.
     *
     * @return The root filesystem path.
     */
    const std::string& getRoot() const;

    /**
     * @brief Returns the default index file name.
     *
     * @return The index file name.
     */
    const std::string& getIndex() const;

    /**
     * @brief Returns whether autoindexing is enabled.
     *
     * @return True if enabled, false otherwise.
     */
    bool isAutoindexEnabled() const;

    /**
     * @brief Checks if a redirect is configured.
     *
     * @return True if a redirect target is set.
     */
    bool hasRedirect() const;

    /**
     * @brief Returns the redirect target.
     *
     * @return The redirection URL.
     */
    const std::string& getRedirect() const;

    /**
     * @brief Returns the HTTP return code for redirection.
     *
     * @return The HTTP status code (e.g., 301).
     */
    int getReturnCode() const;

    /**
     * @brief Returns the upload directory path.
     *
     * @return The path where uploads should be stored.
     */
    const std::string& getUploadStore() const;

    /**
     * @brief Returns the CGI file extension for this location.
     *
     * @return The extension string (e.g., ".php").
     */
    const std::string& getCgiExtension() const;

    // --- Logic helpers ---

    /**
     * @brief Checks if a method is allowed for this location.
     *
     * @param method The HTTP method to check.
     * @return True if allowed, false otherwise.
     */
    bool allowsMethod(const std::string& method) const;

private:
    std::string path_;             ///< The URL path this location matches.

    /**
     * @brief Set of allowed HTTP methods for this location.
     *
     * @details Uses std::set instead of std::vector to:
     * - Prevent duplicate methods automatically.
     * - Allow fast O(log n) lookup with .count().
     * - Reflect that order and duplicates are irrelevant for method checks.
     *
     * This is more semantically correct and efficient than std::vector
     * for the use case of validating allowed methods (e.g., GET, POST, DELETE).
     */
    std::set<std::string> methods_;
    std::string root_;             ///< Root directory for file serving.
    std::string index_;            ///< Default index file name.
    bool autoindex_;               ///< Whether to enable directory listing.
    std::string redirect_;         ///< Redirection target URL.
    int return_code_;              ///< HTTP status code for redirection.
    std::string upload_store_;     ///< Directory for uploaded files.
    std::string cgi_extension_;    ///< CGI file extension (e.g., ".php").
};

/** @} */
