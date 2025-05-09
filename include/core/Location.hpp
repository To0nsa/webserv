/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Location.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/02 13:45:05 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/08 16:36:34 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Location.hpp
 * @brief   Defines the Location class used for per-path server configuration.
 *
 * @details
 * The Location class represents a configuration block associated with a specific
 * URI path on a virtual host. It encapsulates all directives relevant to a given
 * path context, including:
 *
 * - HTTP methods allowed for the path
 * - Filesystem root for serving files
 * - Autoindexing (directory listing) toggle
 * - Index file resolution for directories
 * - Redirection target and code
 * - Upload directory for incoming file data
 * - CGI file extensions triggering script execution
 *
 * This class provides the core logic for evaluating request URIs, matching them
 * to a Location, checking allowed methods, resolving paths, and validating CGI
 * applicability. It is a fundamental part of per-location routing and behavior.
 *
 * @ingroup config
 */

#pragma once

#include <set>
#include <string>
#include <vector>

/**
 * @defgroup config Server Configuration
 * @brief Configuration classes and data structures.
 *
 * This module defines all components used to represent parsed configuration blocks,
 * such as server-wide and per-location settings. Each configuration object encapsulates
 * logic needed to interpret directives and apply them to HTTP requests.
 * @{ */

/**
 * @brief Represents a location block in the server configuration.
 *
 * @details
 * Each Location object is used to define route-specific configuration within a
 * server block. It governs how requests matching a specific URI prefix are handled,
 * including root resolution, allowed methods, redirection, autoindexing, upload
 * paths, and CGI behavior.
 *
 * Example:
 * ```nginx
 * location /images/ {
 *     root /data;
 *     autoindex on;
 *     methods GET POST;
 *     cgi_extension .py;
 * }
 * ```
 *
 * @ingroup config
 */
class Location {
  public:
    ////////////////////////////////
    // --- Constructor
    /**
     * @brief Constructs a Location block with default settings.
     *
     * @details Initializes a new location with autoindex disabled, no redirection,
     *          empty root/index paths, and no allowed methods. The path must be set
     *          explicitly via `setPath()` after construction during parsing.
     *
     * @ingroup config
     */
    Location();
    ~Location()                                = default;
    Location(const Location& other)            = default;
    Location& operator=(const Location& other) = default;

    ///////////////////
    // --- Setters ---
    /**
     * @brief Sets the URI path that this location block matches.
     *
     * @details Defines the request URI prefix (e.g., "/api") used to associate
     *          incoming requests with this location. This path must be unique among
     *          sibling locations in a server block and must begin with a '/'.
     *
     * @param path The URI path prefix to match against incoming requests.
     * @ingroup config
     */
    void setPath(const std::string& path);
    /**
     * @brief Sets the root directory for this location.
     *
     * @details Defines the base directory on the filesystem that corresponds to the
     *          URI path of this location block. All relative request URIs will be
     *          resolved against this root when serving static files. This does not
     *          imply any chroot isolation or sandboxing.
     *
     * @param root Absolute or relative filesystem path to use as the root directory.
     * @ingroup config
     */
    void setRoot(const std::string& root);
    /**
     * @brief Enables or disables directory autoindexing.
     *
     * @details When enabled, if a request targets a directory and no index file
     *          is found, the server will generate a directory listing. If disabled,
     *          such requests will result in a 403 Forbidden response.
     *
     * @param enabled Set to `true` to enable autoindexing, `false` to disable it.
     * @ingroup config
     */
    void setAutoindex(bool enabled);
    /**
     * @brief Sets an HTTP redirection rule for this location.
     *
     * @details Configures an HTTP redirect so that any request matching this
     *          location results in a `code` redirect to the specified `target`.
     *          Common use cases include permanent (301) or temporary (302) redirects.
     *
     * @param target The URI or URL to which the client should be redirected.
     * @param code   The HTTP status code to use (default is 301 for permanent redirect).
     * @ingroup config
     */
    void setRedirect(const std::string& target, int code = 301);
    /**
     * @brief Sets the upload storage path for this location.
     *
     * @details Defines the filesystem directory where uploaded files should be saved
     *          when file upload is enabled for this location. If not set, uploads are disabled.
     *
     * @param path Absolute or relative path to the upload directory.
     * @ingroup config
     */
    void setUploadStore(const std::string& path);
    /**
     * @brief Sets the upload storage directory for this location.
     *
     * @details Defines the target directory where uploaded files will be stored
     *          when file upload handling is enabled. If unset, file uploads to this
     *          location are rejected with an appropriate error (e.g., 403 or 501).
     *
     * @param path Filesystem path to the upload destination directory.
     * @ingroup config
     */
    void addCgiExtension(const std::string& ext);
    /**
     * @brief Adds an allowed HTTP method for this location.
     *
     * @details Registers a specific HTTP method (e.g., "GET", "POST") as permitted
     *          within this location block. Multiple calls accumulate accepted methods.
     *          Methods are matched case-sensitively and validated during parsing.
     *
     * @param method The HTTP method to allow (e.g., "GET", "DELETE", "POST").
     * @ingroup config
     */
    void addMethod(const std::string& method);
    /**
     * @brief Adds an index file candidate for this location.
     *
     * @details Appends a filename to the list of index files to check when a client
     *          requests a directory URI. The server will try each index in order
     *          and serve the first one found in the resolved root path.
     *
     * @param idx The name of the index file to append (e.g., "index.html").
     * @ingroup config
     */
    void addIndexFile(const std::string& idx);

    ///////////////
    // --- Getters
    /**
     * @brief Returns the URI path associated with this location.
     *
     * @details This path determines which incoming requests are matched to this
     *          location block during request routing. It typically begins with '/'.
     *
     * @return Reference to the location's URI path.
     * @ingroup config
     */
    const std::string& getPath() const;
    /**
     * @brief Returns the set of allowed HTTP methods for this location.
     *
     * @details These methods define which HTTP verbs (e.g., GET, POST, DELETE)
     *          are accepted for requests matching this location. The list is
     *          populated from the `methods` directive in the configuration file.
     *
     * @return Reference to the set of allowed HTTP methods.
     * @ingroup config
     */
    const std::set<std::string>& getMethods() const;
    /**
     * @brief Returns the root filesystem directory for this location.
     *
     * @details This path is used to resolve request URIs into actual file paths.
     *          It defines the base directory under which all relative resources
     *          for this location are located.
     *
     * @return Reference to the root directory path.
     * @ingroup config
     */
    const std::string& getRoot() const;
    /**
     * @brief Returns the first configured index file.
     *
     * @details This is a convenience alias for retrieving the highest-priority
     *          index file. If no index files are configured, returns an empty string.
     *
     * @return Reference to the first index file, or an empty string if none exists.
     * @ingroup config
     */
    const std::string& getIndex() const;
    /**
     * @brief Returns the list of all configured index files.
     *
     * @details Index files define fallback filenames to serve when a request
     *          targets a directory. The server will attempt them in order.
     *
     * @return Reference to the list of index filenames.
     * @ingroup config
     */
    const std::vector<std::string>& getIndexFiles() const;
    /**
     * @brief Checks whether directory autoindexing is enabled.
     *
     * @details When enabled, the server will generate an HTML listing if a request
     *          targets a directory and no index file is found. When disabled, such
     *          requests will typically return a 403 Forbidden response.
     *
     * @return `true` if autoindexing is enabled, `false` otherwise.
     * @ingroup config
     */
    bool isAutoindexEnabled() const;
    /**
     * @brief Checks whether a redirection is configured for this location.
     *
     * @details This method returns `true` if a redirect target and status code
     *          have been set via the `return` directive. Used to determine whether
     *          a request should be short-circuited with an HTTP redirect response.
     *
     * @return `true` if a redirection is configured, `false` otherwise.
     * @ingroup config
     */
    bool hasRedirect() const;
    /**
     * @brief Returns the redirection target URI for this location.
     *
     * @details This is the URI or URL to which matching requests should be redirected,
     *          as defined by the `return` directive. If no redirect is set, this returns an empty
     * string.
     *
     * @return Reference to the redirect target string.
     * @ingroup config
     */
    const std::string& getRedirect() const;
    /**
     * @brief Returns the HTTP status code used for redirection.
     *
     * @details This code is returned to the client when a redirect is configured
     *          via the `return` directive (e.g., 301 for permanent, 302 for temporary).
     *          If no redirection is set, this value is 0.
     *
     * @return The HTTP status code for the redirect response.
     * @ingroup config
     */
    int getReturnCode() const;
    /**
     * @brief Returns the upload storage path for this location.
     *
     * @details This is the directory where uploaded files will be stored
     *          if file uploads are enabled for this location. If unset,
     *          uploads are considered disabled and rejected.
     *
     * @return Reference to the upload directory path.
     * @ingroup config
     */
    const std::string& getUploadStore() const;
    /**
     * @brief Returns the first registered CGI extension.
     *
     * @details Used for compatibility with legacy code paths expecting a single CGI extension.
     *          Returns an empty string if no extensions are set.
     *
     * @return Reference to the first CGI extension string, or an empty string if none.
     * @ingroup config
     */
    const std::string& getCgiExtension() const;
    /**
     * @brief Returns all CGI extensions registered for this location.
     *
     * @details These extensions are used to determine whether a request should be handled
     *          via a CGI script. If the requested URI ends with one of the listed extensions,
     *          the request is treated as a CGI request.
     *
     * @return Reference to the vector of CGI file extensions.
     * @ingroup config
     */
    const std::vector<std::string>& getCgiExtensions() const;

    /////////////////////
    // --- Logic helpers
    /**
     * @brief Checks whether a given HTTP method is allowed for this location.
     *
     * @details Compares the input method against the set of methods defined via
     *          the `methods` directive. Matching is case-sensitive and exact.
     *
     * @param method The HTTP method to check (e.g., "GET", "POST").
     * @return `true` if the method is allowed, `false` otherwise.
     * @ingroup config
     */
    bool isMethodAllowed(const std::string& method) const;
    /**
     * @brief Checks whether the given URI matches this location's path.
     *
     * @details Returns `true` if the URI starts with the location's path prefix.
     *          This is used during routing to select the appropriate location block
     *          for an incoming request.
     *
     * @param uri The request URI to compare against this location's path.
     * @return `true` if the URI matches this location, `false` otherwise.
     * @ingroup config
     */
    bool matchesPath(const std::string& uri) const;
    /**
     * @brief Resolves a request URI into an absolute filesystem path.
     *
     * @details If the URI matches this location's path, it is stripped of the prefix
     *          and appended to the root directory to form the full path on disk.
     *          Returns an empty string if the URI does not match this location.
     *
     * @param uri The request URI to resolve.
     * @return The corresponding absolute path on the filesystem, or an empty string if unmatched.
     * @ingroup config
     */
    std::string resolveAbsolutePath(const std::string& uri) const;
    /**
     * @brief Checks whether file uploads are enabled for this location.
     *
     * @details Uploads are considered enabled if an upload store path has been set
     *          via the `upload_store` directive. If not set, file upload requests
     *          will be rejected.
     *
     * @return `true` if uploads are enabled, `false` otherwise.
     * @ingroup config
     */
    bool isUploadEnabled() const;
    /**
     * @brief Checks whether a request should be handled via CGI.
     *
     * @details Returns `true` if the given URI ends with one of the registered
     *          CGI extensions for this location (e.g., ".php", ".py"). This method
     *          is used during request dispatch to determine if CGI execution is required.
     *
     * @param uri The request URI to evaluate.
     * @return `true` if the URI matches a CGI extension, `false` otherwise.
     * @ingroup config
     */
    bool isCgiRequest(const std::string& uri) const;
    /**
     * @brief Computes the full filesystem path to the effective index file.
     *
     * @details If at least one index file is configured, this returns the path formed
     *          by appending the first index filename to the root directory. If no index
     *          files are set, returns an empty string.
     *
     * @return Absolute path to the effective index file, or an empty string if none is configured.
     * @ingroup config
     */
    std::string getEffectiveIndexPath() const;

  private:
    std::string              _path;           ///< URL path this location matches.
    std::set<std::string>    _methods;        ///< Set of allowed HTTP methods.
    std::string              _root;           ///< Root directory for file serving.
    bool                     _autoindex;      ///< Whether to enable directory listing.
    std::string              _redirect;       ///< Redirection target URL.
    int                      _return_code;    ///< HTTP status code for redirection.
    std::string              _upload_store;   ///< Directory for uploaded files.
    std::vector<std::string> _index_files;    ///< Ordered list of index files.
    std::vector<std::string> _cgi_extensions; ///< Ordered list of CGI extensions.
};

/** @} */
