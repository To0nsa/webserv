/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   filesystemUtils.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/13 09:39:07 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/13 09:54:47 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "utilis/filesystemUtils.hpp"
#include "http/HttpResponseBuilder.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <map>

bool isDirectory(const std::string& path) {
    return std::filesystem::is_directory(path);
}

bool fileExists(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

std::string detectMimeType(const std::string& file_path) {
    // Static MIME type mapping: file extension → MIME type
    static const std::map<std::string, std::string> mime_types = {
        {".html", "text/html"},
        {".htm", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".txt", "text/plain"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".png", "image/png"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".pdf", "application/pdf"},
        {".zip", "application/zip"},
        {".tar", "application/x-tar"},
        {".xml", "application/xml"},
        {".mp3", "audio/mpeg"},
        {".mp4", "video/mp4"},
        {".wasm", "application/wasm"}
    };

    // Extract the file extension from the path (e.g., ".html")
    std::filesystem::path path(file_path);
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
    std::string content_type)
{
    // Check if the file exists and is a regular file (not a directory, socket, etc.)
    if (!fileExists(file_path)) {
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