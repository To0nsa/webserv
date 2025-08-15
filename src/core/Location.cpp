/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Location.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:45:32 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/15 23:14:13 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Location.cpp
 * @brief   Implements the Location class for route-specific configuration.
 *
 * @details Defines all methods for @ref Location, including setters, getters,
 *          and logic helpers for request routing and filesystem resolution.
 *          A Location manages path matching, allowed methods, redirection,
 *          CGI parameters, index files, autoindexing, and upload storage.
 *
 * @ingroup location_component
 */

#include "core/Location.hpp"         // Class declaration
#include "utils/filesystemUtils.hpp" // normalizePath(), joinPath()
#include "utils/stringUtils.hpp"     // toLower()
#include <algorithm>                 // std::find
#include <filesystem>                // std::filesystem::path
#include <map>
#include <vector>

//=== Construction & Special Members =====================================

/**
 * @brief Constructs a Location with safe defaults.
 *
 * @details Initializes autoindex to `false` and return code to `0`.
 *          Other members are left empty until populated by configuration.
 *
 * @ingroup location_component
 */
Location::Location() : _autoindex(false), _return_code(0) {
}

//=== Configuration Setters ==============================================

/**
 * @brief Sets the URI path this location matches.
 *
 * @param path Canonical route prefix (e.g., "/images").
 *
 * @ingroup location_component
 */
void Location::setPath(const std::string& path) {
    _path = path;
}

/**
 * @brief Adds a single allowed HTTP method.
 *
 * @param method Method name (e.g., "GET", "POST").
 *
 * @ingroup location_component
 */
void Location::addMethod(const std::string& method) {
    _methods.insert(method);
}

/**
 * @brief Replaces the set of allowed HTTP methods.
 *
 * @param methods List of allowed methods.
 *
 * @ingroup location_component
 */
void Location::setAllowedMethods(const std::vector<std::string>& methods) {
    _methods.clear();
    for (std::vector<std::string>::const_iterator it = methods.begin(); it != methods.end(); ++it)
        addMethod(*it);
}

/**
 * @brief Sets the filesystem root for this location.
 *
 * @param root Directory path.
 *
 * @ingroup location_component
 */
void Location::setRoot(const std::string& root) {
    _root = root;
}

/**
 * @brief Appends an index file candidate in preference order.
 *
 * @param idx Filename (e.g., "index.html").
 *
 * @ingroup location_component
 */
void Location::addIndexFile(const std::string& idx) {
    _index_files.push_back(idx);
}

/**
 * @brief Enables or disables directory listing.
 *
 * @param enabled `true` to enable, `false` to disable.
 *
 * @ingroup location_component
 */
void Location::setAutoindex(bool enabled) {
    _autoindex = enabled;
}

/**
 * @brief Configures an HTTP redirect.
 *
 * @param target Target URL.
 * @param code   HTTP status code (default 301).
 *
 * @ingroup location_component
 */
void Location::setRedirect(const std::string& target, int code) {
    _redirect    = target;
    _return_code = code;
}

/**
 * @brief Sets the upload store directory.
 *
 * @param path Filesystem path for uploaded files.
 *
 * @ingroup location_component
 */
void Location::setUploadStore(const std::string& path) {
    _upload_store = path;
}

/**
 * @brief Adds a CGI extension.
 *
 * @param ext Extension (e.g., ".php").
 *
 * @ingroup location_component
 */
void Location::addCgiExtension(const std::string& ext) {
    _cgi_extensions.push_back(ext);
}

/**
 * @brief Associates a CGI extension with its interpreter.
 *
 * @param ext  Extension (e.g., ".py").
 * @param path Interpreter path.
 *
 * @ingroup location_component
 */
void Location::addCgiInterpreter(const std::string& ext, const std::string& path) {
    _cgi_interpreters[ext] = path;
}

//=== Queries (Getters) ===================================================

/**
 * @brief Returns the path this location matches.
 *
 * @return Reference to the path string.
 *
 * @ingroup location_component
 */
const std::string& Location::getPath() const {
    return _path;
}

/**
 * @brief Returns the allowed HTTP methods.
 *
 * @return Set of method strings.
 *
 * @ingroup location_component
 */
const std::set<std::string>& Location::getMethods() const {
    return _methods;
}

/**
 * @brief Returns the root directory path.
 *
 * @return Reference to root string.
 *
 * @ingroup location_component
 */
const std::string& Location::getRoot() const {
    return _root;
}

/**
 * @brief Returns the first index filename, or empty string.
 *
 * @return Reference to filename or empty string.
 *
 * @ingroup location_component
 */
const std::string& Location::getIndex() const {
    static const std::string empty;
    return _index_files.empty() ? empty : _index_files.front();
}

/**
 * @brief Returns all index filenames.
 *
 * @return Vector of filenames.
 *
 * @ingroup location_component
 */
const std::vector<std::string>& Location::getIndexFiles() const {
    return _index_files;
}

/**
 * @brief Checks if autoindexing is enabled.
 *
 * @return `true` if enabled.
 *
 * @ingroup location_component
 */
bool Location::isAutoindexEnabled() const {
    return _autoindex;
}

/**
 * @brief Checks if a redirect is configured.
 *
 * @return `true` if redirect is set.
 *
 * @ingroup location_component
 */
bool Location::hasRedirect() const {
    return !_redirect.empty();
}

/**
 * @brief Returns the redirect target.
 *
 * @return Reference to redirect URL.
 *
 * @ingroup location_component
 */
const std::string& Location::getRedirect() const {
    return _redirect;
}

/**
 * @brief Returns the redirect HTTP status code.
 *
 * @return HTTP code.
 *
 * @ingroup location_component
 */
int Location::getReturnCode() const {
    return _return_code;
}

/**
 * @brief Returns the upload store directory.
 *
 * @return Reference to upload path.
 *
 * @ingroup location_component
 */
const std::string& Location::getUploadStore() const {
    return _upload_store;
}

/**
 * @brief Returns the first CGI extension, or empty string.
 *
 * @return Reference to extension string.
 *
 * @ingroup location_component
 */
const std::string& Location::getCgiExtension() const {
    static const std::string empty;
    return _cgi_extensions.empty() ? empty : _cgi_extensions.front();
}

/**
 * @brief Returns all CGI extensions.
 *
 * @return Vector of extensions.
 *
 * @ingroup location_component
 */
const std::vector<std::string>& Location::getCgiExtensions() const {
    return _cgi_extensions;
}

/**
 * @brief Returns the interpreter for a CGI extension.
 *
 * @param ext Extension (case-insensitive, optional dot).
 * @return Interpreter path or empty string.
 *
 * @ingroup location_component
 */
std::string Location::getCgiInterpreter(const std::string& ext) const {
    std::string key = toLower(ext);
    if (!key.empty() && key[0] != '.')
        key = "." + key;

    std::map<std::string, std::string>::const_iterator it = _cgi_interpreters.find(key);
    return (it != _cgi_interpreters.end()) ? it->second : "";
}

/**
 * @brief Returns the CGI extension → interpreter map.
 *
 * @return Map of extension to interpreter path.
 *
 * @ingroup location_component
 */
const std::map<std::string, std::string>& Location::getCgiInterpreterMap() const {
    return _cgi_interpreters;
}

//=== Logic Helpers =======================================================

/**
 * @brief Checks if allowed methods list is non-empty.
 *
 * @return `true` if at least one method is allowed.
 *
 * @ingroup location_component
 */
bool Location::hasAllowedMethods() const {
    return !_methods.empty();
}

/**
 * @brief Checks if a method is allowed.
 *
 * @param method Method name.
 * @return `true` if allowed.
 *
 * @ingroup location_component
 */
bool Location::isMethodAllowed(const std::string& method) const {
    return _methods.count(method) > 0;
}

/**
 * @brief Checks if a URI matches this location's path.
 *
 * @param uri Request URI.
 * @return `true` if URI starts with location path.
 *
 * @ingroup location_component
 */
bool Location::matchesPath(const std::string& uri) const {
    std::string cleanUri = normalizePath(uri);
    std::string locPath  = normalizePath(_path);
    return cleanUri.rfind(locPath, 0) == 0;
}

/**
 * @brief Resolves an absolute filesystem path from a URI.
 *
 * @param uri Request URI.
 * @return Joined filesystem path, or empty on mismatch.
 *
 * @ingroup location_component
 */
std::string Location::resolveAbsolutePath(const std::string& uri) const {
    std::string cleanUri = normalizePath(uri);
    if (!matchesPath(cleanUri) || _path.length() > cleanUri.length())
        return "";
    return joinPath(_root, cleanUri.substr(_path.length()));
}

/**
 * @brief Checks if uploads are enabled.
 *
 * @return `true` if upload store is set.
 *
 * @ingroup location_component
 */
bool Location::isUploadEnabled() const {
    return !_upload_store.empty();
}

/**
 * @brief Checks if a path refers to a CGI request.
 *
 * @param path Filesystem or URI path.
 * @return `true` if extension is in CGI list.
 *
 * @ingroup location_component
 */
bool Location::isCgiRequest(const std::string& path) const {
    std::string ext = std::filesystem::path(path).extension().string();
    return std::find(_cgi_extensions.begin(), _cgi_extensions.end(), ext) != _cgi_extensions.end();
}

/**
 * @brief Returns absolute path to first index file.
 *
 * @return `joinPath(root, index)` or empty if none.
 *
 * @ingroup location_component
 */
std::string Location::getEffectiveIndexPath() const {
    if (_index_files.empty())
        return "";
    return joinPath(_root, _index_files.front());
}
