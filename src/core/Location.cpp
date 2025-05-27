/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Location.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:45:32 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/26 20:46:04 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/Location.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/stringUtils.hpp"
#include <algorithm>
#include <filesystem>
#include <map>
#include <vector>

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

void Location::setAllowedMethods(const std::vector<std::string>& methods) {
    _methods.clear();
    for (std::vector<std::string>::const_iterator it = methods.begin(); it != methods.end(); ++it) {
        addMethod(*it);
    }
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

void Location::addCgiInterpreter(const std::string& ext, const std::string& path) {
    _cgi_interpreters[ext] = path;
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

std::string Location::getCgiInterpreter(const std::string& ext) const {
    std::string key = toLower(ext);
    if (!key.empty() && key[0] != '.')
        key = "." + key;

    std::map<std::string, std::string>::const_iterator it = _cgi_interpreters.find(key);
    return (it != _cgi_interpreters.end()) ? it->second : "";
}

const std::map<std::string, std::string>& Location::getCgiInterpreterMap() const {
    return _cgi_interpreters;
}

/////////////////////
// --- Logic Helpers

bool Location::hasAllowedMethods() const {
    return !_methods.empty();
}

bool Location::isMethodAllowed(const std::string& method) const {
    return _methods.count(method) > 0;
}

bool Location::matchesPath(const std::string& uri) const {
    return normalizePath(uri).rfind(normalizePath(_path), 0) == 0;
}

std::string Location::resolveAbsolutePath(const std::string& uri) const {
    std::string cleanUri = normalizePath(uri);
    if (!matchesPath(cleanUri))
        return "";
    return joinPath(_root, cleanUri.substr(_path.length()));
}

bool Location::isUploadEnabled() const {
    return !_upload_store.empty();
}

bool Location::isCgiRequest(const std::string& path) const {
    std::string ext = std::filesystem::path(path).extension().string();
    return std::find(_cgi_extensions.begin(), _cgi_extensions.end(), ext) != _cgi_extensions.end();
}

std::string Location::getEffectiveIndexPath() const {
    if (_index_files.empty())
        return "";
    return joinPath(_root, _index_files.front());
}