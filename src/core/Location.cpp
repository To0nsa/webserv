/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Location.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:45:32 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/03 15:28:45 by irychkov         ###   ########.fr       */
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
Location::Location() : autoindex_(false), return_code_(0) {
}

// --- Setters ---

void Location::setPath(const std::string& path) {
    path_ = path;
}

void Location::addMethod(const std::string& method) {
    methods_.insert(method);
}

void Location::setRoot(const std::string& root) {
    root_ = root;
}

void Location::setIndex(const std::string& index) {
    index_ = index;
}

void Location::setAutoindex(bool enabled) {
    autoindex_ = enabled;
}

void Location::setRedirect(const std::string& target, int code) {
    redirect_    = target;
    return_code_ = code;
}

void Location::setUploadStore(const std::string& path) {
    upload_store_ = path;
}

void Location::setCgiExtension(const std::string& ext) {
    cgi_extension_ = ext;
}

// --- Getters ---

const std::string& Location::getPath() const {
    return path_;
}

const std::set<std::string>& Location::getMethods() const {
    return methods_;
}

const std::string& Location::getRoot() const {
    return root_;
}

const std::string& Location::getIndex() const {
    return index_;
}

bool Location::isAutoindexEnabled() const {
    return autoindex_;
}

bool Location::hasRedirect() const {
    return !redirect_.empty();
}

const std::string& Location::getRedirect() const {
    return redirect_;
}

int Location::getReturnCode() const {
    return return_code_;
}

const std::string& Location::getUploadStore() const {
    return upload_store_;
}

const std::string& Location::getCgiExtension() const {
    return cgi_extension_;
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
    return methods_.count(method) > 0;
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
    return uri.rfind(path_, 0) == 0; // path_ is a prefix of uri
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
    return root_ + uri.substr(path_.length());
}

/**
 * @brief Indicates whether file uploads are allowed for this location.
 *
 * @return True if an upload directory is configured, false otherwise.
 */
bool Location::isUploadEnabled() const {
    return !upload_store_.empty();
}

/**
 * @brief Determines if the URI targets a CGI script.
 *
 * @param uri The requested URI.
 * @return True if the URI ends with the configured CGI extension.
 */
bool Location::isCgiRequest(const std::string& uri) const {
    return !cgi_extension_.empty() && uri.size() >= cgi_extension_.size() &&
           uri.compare(uri.size() - cgi_extension_.size(), cgi_extension_.size(), cgi_extension_) ==
               0;
}

/**
 * @brief Returns the full path to the index file, if one is set.
 *
 * @return The absolute path to the index file or an empty string.
 */
std::string Location::getEffectiveIndexPath() const {
    return index_.empty() ? "" : root_ + "/" + index_;
}
