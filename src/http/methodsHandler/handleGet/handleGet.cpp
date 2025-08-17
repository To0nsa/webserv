/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleGet.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/15 12:39:41 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/17 12:24:33 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/Location.hpp"         // for Location
#include "http/HttpRequest.hpp"      // for HttpRequest
#include "http/HttpResponse.hpp"     // for HttpResponse
#include "http/methodsHandler.hpp"   // for generateAutoindex, handleGet
#include "http/responseBuilder.hpp"  // for generateError, generateRedirect
#include "utils/Logger.hpp"          // for LogLevel, Logger
#include "utils/filesystemUtils.hpp" // for isFile, isSymlink, joinPath
#include <algorithm>                 // for transform
#include <cctype>                    // for tolower
#include <filesystem>                // for path
#include <fstream>                   // for basic_ifstream, basic_ios, ios
#include <map>                       // for map, operator==, _Rb_tree_const...
#include <sstream>                   // for basic_ostringstream
#include <string>                    // for allocator, operator+, char_traits
#include <sys/stat.h>                // for stat, S_ISDIR, S_ISREG
#include <utility>                   // for pair
class Server;

namespace {

static std::string detectMimeType(const std::string& file_path) {
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

    std::filesystem::path fsPath(file_path);
    std::string           ext = fsPath.extension().string();

    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    auto it = mime_types.find(ext);
    if (it != mime_types.end())
        return it->second;

    return "application/octet-stream";
}

static HttpResponse serveFile(const std::string& file_path, const HttpRequest& request,
                              const Server& server, std::string content_type) {
    if (!isFile(file_path)) {
        Logger::logFrom(LogLevel::WARN, "Get Handler", "File not found: " + file_path);
        return ResponseBuilder::generateError(404, server, request);
    }

    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        Logger::logFrom(LogLevel::WARN, "Get Handler", "Cannot open file: " + file_path);
        return ResponseBuilder::generateError(403, server, request);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (content_type.empty())
        content_type = detectMimeType(file_path);

    constexpr std::streamsize MEMORY_LIMIT = 100 * 1024;
    if (size <= MEMORY_LIMIT) {
        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string body = buffer.str();

        Logger::logFrom(LogLevel::INFO, "Get Handler",
                        "Serving small file in-memory (size: " + std::to_string(size) +
                            " bytes): " + file_path);
        return ResponseBuilder::generateSuccess(200, body, content_type, request);
    } else {
        Logger::logFrom(LogLevel::INFO, "Get Handler",
                        "Serving large file via streaming (size: " + std::to_string(size) +
                            " bytes): " + file_path);
        return ResponseBuilder::generateSuccessFile(200, file_path, content_type, request, size);
    }
}

static HttpResponse processDirectory(const std::string& dirPath, const std::string& uri,
                                     const HttpRequest& request, const Server& server,
                                     const Location& loc) {
    std::string normalized = normalizePath(uri);
    if (normalized.empty()) {
        Logger::logFrom(LogLevel::WARN, "Get Handler",
                        "Normalized URI is empty (possible escape) for URI: " + uri);
        return ResponseBuilder::generateError(403, server, request);
    }

    if (normalized.back() != '/') {
        std::string target = normalized + "/";
        Logger::logFrom(LogLevel::INFO, "Get Handler", "Redirecting to: " + target);
        return ResponseBuilder::generateRedirect(301, target, request);
    }

    const std::string& indexName = loc.getIndex();
    if (!indexName.empty()) {
        std::string indexFullPath = joinPath(dirPath, indexName);
        if (isFile(indexFullPath)) {
            return serveFile(indexFullPath, request, server, "");
        }

        if (loc.isAutoindexEnabled()) {
            return generateAutoindex(dirPath, uri, request, server);
        }

        Logger::logFrom(LogLevel::WARN, "Get Handler",
                        "Index file not found and autoindex disabled for: " + dirPath);
        return ResponseBuilder::generateError(403, server, request);
    }

    if (loc.isAutoindexEnabled()) {
        return generateAutoindex(dirPath, uri, request, server);
    }

    Logger::logFrom(LogLevel::WARN, "Get Handler",
                    "Directory listing not allowed (autoindex disabled) for: " + dirPath);
    return ResponseBuilder::generateError(403, server, request);
}

} // namespace

HttpResponse handleGet(const HttpRequest& request, const Server& server, const Location& loc) {
    std::string filepath = resolvePhysicalPath(request, loc);

    // NGINX: any GET ending in '/' is a directory lookup.
    // If stripping the slash yields an existing file, return 404.
    const std::string& uri = request.getPath();
    if (!uri.empty() && uri.back() == '/') {
        std::string fileNoSlash = filepath;
        if (!fileNoSlash.empty() && fileNoSlash.back() == '/')
            fileNoSlash.pop_back();
        if (isFile(fileNoSlash)) {
            Logger::logFrom(LogLevel::WARN, "Get Handler",
                            "Trailing slash on file → returning 404 for URI: " + uri);
            return ResponseBuilder::generateError(404, server, request);
        }
    }

    if (isSymlink(filepath)) {
        Logger::logFrom(LogLevel::WARN, "Get Handler",
                        "Symlink detected, rejecting request for: " + filepath);
        return ResponseBuilder::generateError(403, server, request);
    }

    struct stat fileStat;
    if (stat(filepath.c_str(), &fileStat) == 0) {
        const std::string& uri = request.getPath();

        if (S_ISDIR(fileStat.st_mode)) {
            return processDirectory(filepath, uri, request, server, loc);
        }

        if (S_ISREG(fileStat.st_mode)) {
            return serveFile(filepath, request, server, "");
        }
    }

    Logger::logFrom(LogLevel::WARN, "Get Handler", "File not found or inaccessible: " + filepath);
    return ResponseBuilder::generateError(404, server, request);
}
