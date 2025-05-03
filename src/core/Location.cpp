/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Location.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:45:32 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/03 15:36:52 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Location.cpp
 * @brief   Implements the Location class methods.
 *
 * @details Provides setters, getters, and helper logic for path-based configuration
 * blocks, including method filtering, path resolution, CGI detection, and more.
 * @ingroup core
 */

#include "core/Location.hpp"

// --- Constructor ---

/**
 * @brief Constructs a Location with default values.
 *
 * @details Initializes autoindex to false and return code to 0.
 */
Location::Location() : _autoindex(false), _return_code(0) {
}

// --- Setters ---

void Location::setPath(const std::string& path) {
    _path = path;
}

void Location::addMethod(const std::string& method) {
    _methods.insert(method);
}

void Location::setRoot(const std::string& root) {
    _root = root;
}

void Location::setIndex(const std::string& index) {
    _index = index;
}

void Location::setAutoindex(bool enabled) {
    _autoindex = enabled;
}

void Location::setRedirect(const std::string& target, int code) {
    _redirect    = target;
    _return_code = code;
}

void Location::setUploadStore(const std::string& path) {
    _upload_store = path;
}

void Location::setCgiExtension(const std::string& ext) {
    _cgi_extension = ext;
}

// --- Getters ---

const std::string& Location::getPath() const {
    return _path;
}

const std::set<std::string>& Location::getMethods() const {
    return _methods;
}

const std::string& Location::getRoot() const {
    return _root;
}

const std::string& Location::getIndex() const {
    return _index;
}

bool Location::isAutoindexEnabled() const {
    return _autoindex;
}

bool Location::hasRedirect() const {
    return !_redirect.empty();
}

const std::string& Location::getRedirect() const {
    return _redirect;
}

int Location::getReturnCode() const {
    return _return_code;
}

const std::string& Location::getUploadStore() const {
    return _upload_store;
}

const std::string& Location::getCgiExtension() const {
    return _cgi_extension;
}

// --- Logic Helpers ---

/**
 * @brief Verifies if the given HTTP method is allowed for this location.
 *
 * @details Checks whether the method exists in the set of allowed methods
 * configured for this location (e.g., "GET", "POST", "DELETE").
 *
 * @param method The HTTP method string to check.
 * @return True if the method is allowed, false otherwise.
 */
bool Location::isMethodAllowed(const std::string& method) const {
    return _methods.count(method) > 0;
}

/**
 * @brief Checks if this location matches a given request URI.
 *
 * @details This is typically a prefix match. For example, if the path is "/api",
 * then this method returns true for "/api/users" and "/api/status".
 *
 * @param uri The full request URI to match.
 * @return True if the location path is a prefix of the URI.
 */
bool Location::matchesPath(const std::string& uri) const {
    return uri.rfind(_path, 0) == 0; // _path is a prefix of uri
}

/**
 * @brief Resolves a request URI into a full filesystem path.
 *
 * @details Joins the root directory and the URI suffix after the location path.
 * Returns an empty string if the URI does not match this location.
 *
 * @param uri The full request URI.
 * @return The absolute path to the requested resource.
 */
std::string Location::resolveAbsolutePath(const std::string& uri) const {
    if (!matchesPath(uri))
        return "";
    return _root + uri.substr(_path.length());
}

/**
 * @brief Indicates whether file uploads are allowed for this location.
 *
 * @return True if an upload directory is configured, false otherwise.
 */
bool Location::isUploadEnabled() const {
    return !_upload_store.empty();
}

/**
 * @brief Determines if the URI targets a CGI script.
 *
 * @param uri The requested URI.
 * @return True if the URI ends with the configured CGI extension.
 */
bool Location::isCgiRequest(const std::string& uri) const {
    return !_cgi_extension.empty() && uri.size() >= _cgi_extension.size() &&
           uri.compare(uri.size() - _cgi_extension.size(), _cgi_extension.size(), _cgi_extension) ==
               0;
}

/**
 * @brief Returns the full path to the index file, if one is set.
 *
 * @return The absolute path to the index file or an empty string.
 */
std::string Location::getEffectiveIndexPath() const {
    return _index.empty() ? "" : _root + "/" + _index;
}
