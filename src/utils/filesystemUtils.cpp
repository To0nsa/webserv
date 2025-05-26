/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   filesystemUtils.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/13 09:39:07 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/26 13:41:29 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "utils/filesystemUtils.hpp"
#include "http/HttpResponseBuilder.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

bool isFile(const std::string& path) {
    return fs::exists(path) && fs::is_regular_file(path);
}

std::string detectMimeType(const std::string& file_path) {
    // Static MIME type mapping: file extension → MIME type
    static const std::map<std::string, std::string> mime_types = {
        {".html", "text/html"},        {".htm", "text/html"},
        {".css", "text/css"},          {".js", "application/javascript"},
        {".json", "application/json"}, {".txt", "text/plain"},
        {".jpg", "image/jpeg"},        {".jpeg", "image/jpeg"},
        {".png", "image/png"},         {".gif", "image/gif"},
        {".svg", "image/svg+xml"},     {".ico", "image/x-icon"},
        {".pdf", "application/pdf"},   {".zip", "application/zip"},
        {".tar", "application/x-tar"}, {".xml", "application/xml"},
        {".mp3", "audio/mpeg"},        {".mp4", "video/mp4"},
        {".wasm", "application/wasm"}};

    // Extract the file extension from the path (e.g., ".html")
    fs::path    path(file_path);
    std::string ext = path.extension().string();

    // Convert the extension to lowercase to ensure case-insensitive matching
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Look up the extension in the MIME type map
    std::map<std::string, std::string>::const_iterator it = mime_types.find(ext);
    if (it != mime_types.end())
        return it->second; // Known type found

    // Fallback: return generic binary stream for unknown extensions
    return "application/octet-stream";
}

HttpResponse serveFile(const std::string& file_path, const HttpRequest& request,
                       std::string content_type) {
    // Check if the file exists and is a regular file (not a directory, socket, etc.)
    if (!isFile(file_path)) {
        return ResponseBuilder::generateError(404, Server(), request);
    }

    // Open the file in binary mode to avoid any platform-specific transformations
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        // File exists but can't be opened (permissions, locked, etc.)
        return ResponseBuilder::generateError(403, Server(), request);
    }

    // Read the full content of the file into a string
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string body = buffer.str();

    // If no content type was explicitly passed, detect it from file extension
    if (content_type.empty()) {
        content_type = detectMimeType(file_path);
    }

    // Return a successful HTTP response with the file content and correct MIME type
    return ResponseBuilder::generateSuccess(200, body, content_type, request);
}

std::string normalizePath(const std::string& path) {
    return fs::path(path).lexically_normal().string();
}

std::string joinPath(const std::string& base, const std::string& suffix) {
    return (fs::path(base) / suffix).lexically_normal().string();
}

std::string buildFilePath(const HttpRequest& request, const Location& loc) {
    fs::path req       = request.getPath();
    fs::path locPrefix = loc.getPath();
    fs::path locRoot   = loc.getRoot();

    fs::path suffix = req.lexically_relative(locPrefix);

    if (suffix == "." || suffix.empty()) {
        suffix.clear();
    }

    fs::path full = (locRoot / suffix).lexically_normal();
    return full.string();
}

bool ensureDirectoryExists(const std::string& path) {
    try {
        return fs::create_directories(path);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[ensureDirectoryExists] Error creating directories: " << e.what() << '\n';
        return false;
    }
}

bool containsTraversal(const std::string& pathStr) {
    fs::path raw = fs::path(pathStr);
    for (const auto& part : raw) {
        if (part == "..")
            return true;
    }
    return false;
}

bool isInvalidAbsolutePath(const std::string& pathStr) {
    if (pathStr.empty())
        return true;

    fs::path raw(pathStr);
    if (!raw.is_absolute())
        return true;

    // Check for `..` anywhere in the original path
    for (const auto& part : raw) {
        if (part == "..")
            return true;
    }

    // Check for segments that *start* with `..` (e.g., `..private`)
    for (const auto& part : raw) {
        const std::string& seg = part.string();
        if (seg.rfind("..", 0) == 0) // starts with ".."
            return true;
    }

    // Redundant slashes in raw input
    if (pathStr.find("//") != std::string::npos)
        return true;

    return false;
}

bool isSuspiciousFilename(const std::string& filename) {
    if (filename.empty() || filename.size() > 256)
        return true;

    fs::path p(filename);

    if (p.has_parent_path() || filename.find('/') != std::string::npos ||
        containsTraversal(filename))
        return true;

    // Must not start with a dot or dash
    if (!std::isalnum(static_cast<unsigned char>(filename[0])))
        return true;

    // Enforce strict whitelist pattern: no multiple dots, only one extension, valid suffix
    static const std::regex strictPattern(R"(^[a-zA-Z0-9_-]+\.(html?|txt|php|cgi)$)");
    return !std::regex_match(filename, strictPattern);
}
