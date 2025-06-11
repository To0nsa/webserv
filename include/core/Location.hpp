/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Location.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/02 13:45:05 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/11 09:12:51 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

class Location {
  public:
    ///////////////////
    // --- Constructor
    Location();
    ~Location()                                = default;
    Location(const Location& other)            = default;
    Location& operator=(const Location& other) = default;

    ///////////////////
    // --- Setters ---
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

    ///////////////
    // --- Getters
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

    /////////////////////
    // --- Logic helpers
    bool        hasAllowedMethods() const;
    bool        isMethodAllowed(const std::string& method) const;
    bool        matchesPath(const std::string& uri) const;
    std::string resolveAbsolutePath(const std::string& uri) const;
    bool        isUploadEnabled() const;
    bool        isCgiRequest(const std::string& uri) const;
    std::string getEffectiveIndexPath() const;

  private:
    std::string                        _path;           ///< URL path this location matches.
    std::set<std::string>              _methods;        ///< Set of allowed HTTP methods.
    std::string                        _root;           ///< Root directory for file serving.
    bool                               _autoindex;      ///< Whether to enable directory listing.
    std::string                        _redirect;       ///< Redirection target URL.
    int                                _return_code;    ///< HTTP status code for redirection.
    std::string                        _upload_store;   ///< Directory for uploaded files.
    std::vector<std::string>           _index_files;    ///< Ordered list of index files.
    std::vector<std::string>           _cgi_extensions; ///< Ordered list of CGI extensions.
    std::map<std::string, std::string> _cgi_interpreters;
};

/** @} */
