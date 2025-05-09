/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Location.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:45:32 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/08 16:45:15 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Location.cpp
 * @brief   Implements the Location class methods.
 *
 * @details Provides setters, getters, and helper logic for path-based configuration
 * blocks, including method filtering, path resolution, CGI detection, and more.
 * @ingroup config
 */

#include "core/Location.hpp"

///////////////////////
// --- Constructor ---

Location::Location() : _autoindex(false), _return_code(0) {
}

///////////////
// --- Setters

void Location::setPath(const std::string& path) {
    _path = path;
}

void Location::addMethod(const std::string& method) {
    _methods.insert(method);
}

void Location::setRoot(const std::string& root) {
    _root = root;
}

void Location::addIndexFile(const std::string& idx) {
    _index_files.push_back(idx);
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

void Location::addCgiExtension(const std::string& ext) {
    _cgi_extensions.push_back(ext);
}

///////////////
// --- Getters

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
    static const std::string empty;
    return _index_files.empty() ? empty : _index_files.front();
}

const std::vector<std::string>& Location::getIndexFiles() const {
    return _index_files;
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
    static const std::string empty;
    return _cgi_extensions.empty() ? empty : _cgi_extensions.front();
}

const std::vector<std::string>& Location::getCgiExtensions() const {
    return _cgi_extensions;
}

/////////////////////
// --- Logic Helpers

bool Location::isMethodAllowed(const std::string& method) const {
    return _methods.count(method) > 0;
}

bool Location::matchesPath(const std::string& uri) const {
    return uri.rfind(_path, 0) == 0;
}

std::string Location::resolveAbsolutePath(const std::string& uri) const {
    if (!matchesPath(uri))
        return "";
    return _root + uri.substr(_path.length());
}

bool Location::isUploadEnabled() const {
    return !_upload_store.empty();
}

bool Location::isCgiRequest(const std::string& uri) const {
    for (const std::string& ext : _cgi_extensions) {
        // Skip empty extensions and ensure URI is long enough
        if (!ext.empty() && uri.size() >= ext.size() &&
            // Check if URI ends with the current CGI extension
            uri.compare(uri.size() - ext.size(), ext.size(), ext) == 0) {
            return true; // Match found: this is a CGI request
        }
    }
    return false; // No matching extension found
}

std::string Location::getEffectiveIndexPath() const {
    if (_index_files.empty())
        return "";
    return _root + "/" + _index_files.front();
}
