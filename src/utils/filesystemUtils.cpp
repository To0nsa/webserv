/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   filesystemUtils.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/13 09:39:07 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/03 21:09:08 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "utils/filesystemUtils.hpp"
#include "http/HttpResponseBuilder.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

bool isFile(const std::string& path) {
    return fs::exists(path) && fs::is_regular_file(path);
}

std::string make_temp_name(const std::string& prefix, unsigned& counter) {
    // 1) Where to put it (e.g. "/tmp" on Linux, or $TMPDIR)
    fs::path tmpdir = fs::temp_directory_path();

    // 2) High-precision timestamp (nanoseconds since epoch)
    auto now = std::chrono::high_resolution_clock::now();
    auto ns  = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    // 3) Build "<prefix>_<pid>_<nanoseconds>_<counter>.tmp"
    std::ostringstream ss;
    ss << prefix << "_" << ns << "_" << counter++ << ".tmp";

    return (tmpdir / ss.str()).string();
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
    // Check if the file exists and is a regular file
    if (!isFile(file_path)) {
        return ResponseBuilder::generateError(404, Server(), request);
    }

    // Try to open file
    std::ifstream file(file_path, std::ios::binary | std::ios::ate); // Open at end to get size
    if (!file.is_open()) {
        return ResponseBuilder::generateError(403, Server(), request);
    }

    std::streamsize size = file.tellg(); // Get file size
    file.seekg(0, std::ios::beg);        // Reset pointer

    // Detect content type if not provided
    if (content_type.empty()) {
        content_type = detectMimeType(file_path);
    }

    // Threshold for in-memory vs streaming (100 KB)
    const std::streamsize MEMORY_LIMIT = 100 * 1024;

    if (size <= MEMORY_LIMIT) {
        // Small file: read into memory
        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string body = buffer.str();
        return ResponseBuilder::generateSuccess(200, body, content_type, request);
    } else {
        // Large file: stream from disk
        return ResponseBuilder::generateSuccessFile(200, file_path, content_type, request, size);
    }
}

/* std::string normalizePath(const std::string& path) {
    if (path.empty())
        return "/";
    std::string result = path;

    if (result.size() > 1 && result.back() == '/')
        result.pop_back();

    return result;
} */

/* std::string normalizePath(const std::string& path) {
    if (path.empty())
        return "/";

    std::vector<std::string> segments;
    std::string              segment;
    std::istringstream       stream(path);
    bool                     hadTrailingSlash = path.back() == '/';

    while (std::getline(stream, segment, '/')) {
        if (segment.empty() || segment == ".")
            continue;
        if (segment == "..") {
            if (!segments.empty()) {
                segments.pop_back(); // move up
            } else {
                // Attempt to go above root: reject this path
                return ""; // special marker for invalid path
            }
        } else {
            segments.push_back(segment);
        }
    }

    std::string result = "/";
    for (std::size_t i = 0; i < segments.size(); ++i) {
        result += segments[i];
        if (i + 1 < segments.size())
            result += "/";
    }

    if (hadTrailingSlash && result != "/")
        result += "/";

    return result;
} */

std::string normalizePath(const std::string& path) {
    if (path.empty())
        return "/";

    // Collapse consecutive slashes into one
    std::string collapsed;
    bool        prevWasSlash = false;
    for (char c : path) {
        if (c == '/') {
            if (!prevWasSlash) {
                collapsed += '/';
                prevWasSlash = true;
            }
        } else {
            collapsed += c;
            prevWasSlash = false;
        }
    }

    std::vector<std::string> segments;
    std::string              segment;
    std::istringstream       stream(collapsed);
    bool                     hadTrailingSlash = collapsed.back() == '/';

    while (std::getline(stream, segment, '/')) {
        if (segment.empty() || segment == ".")
            continue;
        if (segment == "..") {
            if (!segments.empty()) {
                segments.pop_back();
            } else {
                return ""; // invalid traversal
            }
        } else {
            segments.push_back(segment);
        }
    }

    std::string result = "/";
    for (std::size_t i = 0; i < segments.size(); ++i) {
        result += segments[i];
        if (i + 1 < segments.size())
            result += "/";
    }

    if (hadTrailingSlash && result != "/")
        result += "/";

    return result;
}

std::string joinPath(const std::string& base, const std::string& suffix) {
    if (base.empty())
        return suffix;
    if (base.back() == '/')
        return base + suffix;
    return base + '/' + suffix;
}

/* std::string buildFilePath(const HttpRequest& request, const Location& loc) {
    std::string request_path  = normalizePath(request.getPath());
    std::string location_path = normalizePath(loc.getPath());
    std::string location_root = normalizePath(loc.getRoot());

    std::string suffix;
    if (request_path.find(location_path) == 0)
        suffix = request_path.substr(location_path.length());

    if (!suffix.empty() && suffix[0] == '/')
        suffix.erase(0, 1);

    return joinPath(location_root, suffix); // May point to file or directory
} */

std::string buildFilePath(const HttpRequest& request, const Location& loc) {
    // 1) Normalize but PRESERVE trailing-slash info in request URI & location prefix
    std::string req_path = normalizePath(request.getPath());
    std::string loc_path = normalizePath(loc.getPath());
    // 2) Don’t normalize the filesystem root with the same HTTP logic—
    //    use it raw (it should already be an absolute, clean path).
    std::string loc_root = loc.getRoot();

    // 3) Prefix-match exactly at position 0
    if (req_path.rfind(loc_path, 0) != 0) {
        // doesn’t belong to this Location
        return "";
    }

    // 4) Strip the prefix and leading slash from the remainder
    std::string suffix = req_path.substr(loc_path.size());
    if (!suffix.empty() && suffix[0] == '/')
        suffix.erase(0, 1);

    // 5) Join filesystem-style
    return joinPath(loc_root, suffix); // may be file or directory
}

static std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream        ss(path);
    std::string              part;
    while (std::getline(ss, part, '/')) {
        if (!part.empty())
            parts.push_back(part);
    }
    return parts;
}

bool mkdirRecursive(const std::string& path) {
    std::vector<std::string> parts   = splitPath(path);
    std::string              current = path[0] == '/' ? "/" : "";

    for (size_t i = 0; i < parts.size(); ++i) {
        current = joinPath(current, parts[i]);
        if (isFile(current)) {
            std::cerr << "[mkdirRecursive] Path exists and is a file (not directory): " << current
                      << std::endl;
            return false;
        }
        if (mkdir(current.c_str(), 0777) == -1) {
            if (errno != EEXIST) {
                std::cerr << "[mkdirRecursive] Failed to create directory: " << current
                          << " — errno: " << strerror(errno) << std::endl;
                return false;
            }
        }
    }
    return true;
}

std::string decodePercentEncoding(const std::string& encoded) {
    std::ostringstream result;

    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%') {
            if (i + 2 >= encoded.length())
                throw std::invalid_argument("Incomplete percent-encoding at end of URI");

            char hex1 = encoded[i + 1];
            char hex2 = encoded[i + 2];
            if (!isxdigit(hex1) || !isxdigit(hex2))
                throw std::invalid_argument("Invalid hex in percent-encoding: %" +
                                            std::string(1, hex1) + std::string(1, hex2));

            int byte = std::stoi(encoded.substr(i + 1, 2), nullptr, 16);
            result << static_cast<char>(byte);
            i += 2;
        } else {
            result << encoded[i];
        }
    }

    return result.str();
}

bool isSymlink(const std::string& path) {
    return fs::is_symlink(fs::path(path));
}

/* std::string normalizePath(const std::string& path) {
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
} */
