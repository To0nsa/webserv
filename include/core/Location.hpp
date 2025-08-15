/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Location.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/02 13:45:05 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/15 23:15:01 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Location.hpp
 * @brief   Declares the Location class for route-specific configuration.
 *
 * @details Represents a configuration block tied to a specific URL path within
 *          a virtual server. Each Location defines its own root, allowed methods,
 *          redirection rules, upload storage, index files, and CGI settings.
 *          This granularity allows per-path customization of behavior and routing.
 *
 * @ingroup location_component
 */

#pragma once

#include <map>    // std::map for CGI interpreter mapping
#include <set>    // std::set for allowed HTTP methods
#include <string> // std::string for paths and configuration values
#include <vector> // std::vector for index and CGI extension lists

/**
 * @brief Encapsulates configuration for a single URL path.
 *
 * @details Models a location block inside a server configuration. This includes:
 *          - Path matching rules
 *          - File system root for serving content
 *          - Directory listing behavior
 *          - HTTP method restrictions
 *          - Optional redirection
 *          - CGI execution parameters
 *          - Upload handling
 *
 * @ingroup location_component
 */
class Location {
  public:
    //=== Construction & Special Members =====================================

    /** @name Construction & special members */
    ///@{
    Location();
    ~Location()                                = default;
    Location(const Location& other)            = default;
    Location& operator=(const Location& other) = default;
    ///@}

    //=== Configuration Setters ==============================================

    /** @name Configuration setters */
    ///@{
    void setPath(const std::string& path);
    void setRoot(const std::string& root);
    void setAutoindex(bool enabled);
    void setRedirect(const std::string& target, int code = 301);
    void setUploadStore(const std::string& path);
    void addCgiExtension(const std::string& ext);
    void addMethod(const std::string& method);
    void setAllowedMethods(const std::vector<std::string>& methods);
    void addIndexFile(const std::string& idx);
    void addCgiInterpreter(const std::string& ext, const std::string& path);
    ///@}

    //=== Queries (Getters) ===================================================

    /** @name Queries (getters) */
    ///@{
    const std::string&                        getPath() const;
    const std::set<std::string>&              getMethods() const;
    const std::string&                        getRoot() const;
    const std::string&                        getIndex() const;
    const std::vector<std::string>&           getIndexFiles() const;
    bool                                      isAutoindexEnabled() const;
    bool                                      hasRedirect() const;
    const std::string&                        getRedirect() const;
    int                                       getReturnCode() const;
    const std::string&                        getUploadStore() const;
    const std::string&                        getCgiExtension() const;
    const std::vector<std::string>&           getCgiExtensions() const;
    std::string                               getCgiInterpreter(const std::string& ext) const;
    const std::map<std::string, std::string>& getCgiInterpreterMap() const;
    ///@}

    //=== Logic & Matching Helpers ============================================

    /** @name Logic helpers */
    ///@{
    bool        hasAllowedMethods() const;
    bool        isMethodAllowed(const std::string& method) const;
    bool        matchesPath(const std::string& uri) const;
    std::string resolveAbsolutePath(const std::string& uri) const;
    bool        isUploadEnabled() const;
    bool        isCgiRequest(const std::string& uri) const;
    std::string getEffectiveIndexPath() const;
    ///@}

  private:
    //=== Data Members ========================================================

    std::string              _path;                       ///< URL path this location matches.
    std::set<std::string>    _methods;                    ///< Allowed HTTP methods.
    std::string              _root;                       ///< Root directory for file serving.
    bool                     _autoindex;                  ///< Enable/disable directory listing.
    std::string              _redirect;                   ///< Target URL for redirection.
    int                      _return_code;                ///< HTTP status code for redirection.
    std::string              _upload_store;               ///< Directory for file uploads.
    std::vector<std::string> _index_files;                ///< List of default index files.
    std::vector<std::string> _cgi_extensions;             ///< File extensions for CGI execution.
    std::map<std::string, std::string> _cgi_interpreters; ///< CGI extension → interpreter mapping.
};
