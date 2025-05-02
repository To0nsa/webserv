/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Location.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:45:32 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/02 13:59:23 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Location.cpp
 * @brief   Implements the Location class methods.
 *
 * @details Provides setters, getters, and helper logic for path-based configuration
 * blocks, including methods, root directory, index file, CGI extension, and more.
 * @ingroup config
 */

#include "Location.hpp"

// --- Constructor ---

/**
 * @brief Constructs a Location with default values.
 *
 * @details Initializes autoindex to false and return code to 0.
 */
Location::Location()
    : path_(),
    methods_(),
    root_(),
    index_(),
    autoindex_(false),
    redirect_(),
    return_code_(0),
    upload_store_(),
    cgi_extension_() {}

// --- Setters ---

/**
 * @brief Sets the URL path associated with this location.
 *
 * @param path The path to match in the HTTP request (e.g., "/api").
 */
void Location::setPath(const std::string& path) {
    path_ = path;
}

/**
 * @brief Adds an allowed HTTP method.
 *
 * @param method The method to allow (e.g., "GET", "POST").
 */
void Location::addMethod(const std::string& method) {
    methods_.insert(method);
}

/**
 * @brief Sets the root directory for this location.
 *
 * @param root Filesystem path used as root for this location.
 */
void Location::setRoot(const std::string& root) {
    root_ = root;
}

/**
 * @brief Sets the index file name for directory requests.
 *
 * @param index Name of the file to serve (e.g., "index.html").
 */
void Location::setIndex(const std::string& index) {
    index_ = index;
}

/**
 * @brief Enables or disables autoindexing.
 *
 * @param enabled True to enable directory listing.
 */
void Location::setAutoindex(bool enabled) {
    autoindex_ = enabled;
}

/**
 * @brief Sets redirection target and status code.
 *
 * @param target The redirect destination URL.
 * @param code HTTP status code to return (default: 301).
 */
void Location::setRedirect(const std::string& target, int code) {
    redirect_ = target;
    return_code_ = code;
}

/**
 * @brief Sets the path where uploaded files will be stored.
 *
 * @param path Filesystem directory for storing uploads.
 */
void Location::setUploadStore(const std::string& path) {
    upload_store_ = path;
}

/**
 * @brief Sets the CGI extension to trigger script execution.
 *
 * @param ext The CGI file extension (e.g., ".php").
 */
void Location::setCgiExtension(const std::string& ext) {
    cgi_extension_ = ext;
}

// --- Getters ---

/**
 * @brief Returns the configured path.
 *
 * @return Path string (e.g., "/upload").
 */
const std::string& Location::getPath() const {
    return path_;
}

/**
 * @brief Returns the allowed HTTP methods.
 *
 * @return Set of strings (e.g., {"GET", "POST"}).
 */
const std::set<std::string>& Location::getMethods() const {
    return methods_;
}

/**
 * @brief Returns the root directory.
 *
 * @return Filesystem path string.
 */
const std::string& Location::getRoot() const {
    return root_;
}

/**
 * @brief Returns the index file name.
 *
 * @return Index file string.
 */
const std::string& Location::getIndex() const {
    return index_;
}

/**
 * @brief Indicates whether autoindexing is enabled.
 *
 * @return True if enabled, false otherwise.
 */
bool Location::isAutoindexEnabled() const {
    return autoindex_;
}

/**
 * @brief Returns whether a redirection is configured.
 *
 * @return True if redirection is enabled.
 */
bool Location::hasRedirect() const {
    return !redirect_.empty();
}

/**
 * @brief Returns the redirection target.
 *
 * @return Target URL.
 */
const std::string& Location::getRedirect() const {
    return redirect_;
}

/**
 * @brief Returns the redirection HTTP status code.
 *
 * @return Status code (e.g., 301).
 */
int Location::getReturnCode() const {
    return return_code_;
}

/**
 * @brief Returns the upload directory.
 *
 * @return Filesystem path for uploads.
 */
const std::string& Location::getUploadStore() const {
    return upload_store_;
}

/**
 * @brief Returns the configured CGI extension.
 *
 * @return File extension string (e.g., ".py").
 */
const std::string& Location::getCgiExtension() const {
    return cgi_extension_;
}

// --- Logic Helpers ---

/**
 * @brief Verifies if the given HTTP method is allowed for this location.
 *
 * @details This function is typically called during request handling to enforce
 * route-level method restrictions as defined in the configuration file.
 * It checks whether the input method (e.g., "GET", "POST", "DELETE") exists
 * in the `methods_` set, which contains all allowed methods for this path.
 *
 * Method names are expected to be uppercase and matched exactly.
 *
 * @param method The HTTP method string to check (e.g., "GET").
 * @return True if the method is listed in the allowed methods set, false otherwise.
 */
bool Location::allowsMethod(const std::string& method) const {
    return methods_.count(method) > 0;
}

