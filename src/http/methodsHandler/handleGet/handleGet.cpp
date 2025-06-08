/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleGet.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/15 12:39:41 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/09 00:07:17 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>

#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/methodsHandler.hpp"
#include "http/responseBuilder.hpp"
#include "utils/Logger.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/stringUtils.hpp"

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
                              std::string content_type) {
    if (!isFile(file_path)) {
        Logger::logFrom(LogLevel::WARN, "Get Handler", "File not found: " + file_path);
        return ResponseBuilder::generateError(404, Server(), request);
    }

    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        Logger::logFrom(LogLevel::WARN, "Get Handler", "Cannot open file: " + file_path);
        return ResponseBuilder::generateError(403, Server(), request);
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
            return serveFile(indexFullPath, request, "");
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
            return serveFile(filepath, request, "");
        }
    }

    Logger::logFrom(LogLevel::WARN, "Get Handler", "File not found or inaccessible: " + filepath);
    return ResponseBuilder::generateError(404, server, request);
}
