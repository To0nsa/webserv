/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   filesystemUtils.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/13 09:39:07 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/06 21:51:34 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "utils/filesystemUtils.hpp"
#include "http/responseBuilder.hpp"

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
#include <sys/stat.h>

namespace fs = std::filesystem;

std::string resolvePhysicalPath(const HttpRequest& req, const Location& loc) {
    std::string requestPath = normalizePath(req.getPath());
    if (requestPath.empty())
        return "";

    std::string locPrefix     = normalizePath(loc.getPath());
    bool        inUploadStore = loc.isUploadEnabled() && requestPath.rfind(locPrefix, 0) == 0;

    if (inUploadStore) {
        std::string relative = requestPath.substr(locPrefix.size());
        while (!relative.empty() && relative.front() == '/')
            relative.erase(0, 1);

        std::string uploadRoot = normalizePath(loc.getUploadStore());
        if (!uploadRoot.empty() && uploadRoot.front() != '/')
            uploadRoot = joinPath(normalizePath(loc.getRoot()), uploadRoot);

        return joinPath(uploadRoot, relative);
    }

    return buildFilePath(req, loc);
}

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

std::string normalizePath(const std::string& path) {
    if (path.empty())
        return "/";

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
                return "";
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

std::string buildFilePath(const HttpRequest& request, const Location& loc) {
    std::string req_path = normalizePath(request.getPath());
    std::string loc_path = normalizePath(loc.getPath());
    std::string loc_root = loc.getRoot();

    if (req_path.rfind(loc_path, 0) != 0) {
        return "";
    }

    std::string suffix = req_path.substr(loc_path.size());
    if (!suffix.empty() && suffix[0] == '/')
        suffix.erase(0, 1);

    return joinPath(loc_root, suffix);
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
            return false;
        }
        if (mkdir(current.c_str(), 0777) == -1) {
            if (errno != EEXIST) {
                return false;
            }
        }
    }
    return true;
}

bool isSymlink(const std::string& path) {
    return fs::is_symlink(fs::path(path));
}
time_t getCurrentTime() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}