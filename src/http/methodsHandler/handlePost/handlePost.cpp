/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handlePost.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/19 10:19:13 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/06 22:29:41 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/methodsHandler.hpp"
#include "http/responseBuilder.hpp"
#include "utils/Logger.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/urlUtils.hpp"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

static HttpResponse handleUrlEncodedForm(const HttpRequest& request, const Server& server,
                                         const std::string& fullpath, const std::string& filename) {
    auto form = parseFormUrlEncoded(request.getBody());
    if (form.empty()) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "Empty or malformed URL-encoded form body from client.");
        return ResponseBuilder::generateError(400, server, request);
    }

    std::string html = "<html><body><h1>Form Received</h1>";
    for (auto& kv : form) {
        html += "<p><b>" + kv.first + ":</b> " + kv.second + "</p>";
    }
    html += "</body></html>";

    std::ofstream out(fullpath);
    if (!out.is_open()) {
        Logger::logFrom(LogLevel::ERROR, "Post Handler",
                        "Failed to open file for writing: " + fullpath);
        return ResponseBuilder::generateError(500, server, request);
    }

    out << html;
    out.close();
    if (out.fail()) {
        Logger::logFrom(LogLevel::ERROR, "Post Handler",
                        "Failed to write or close file: " + fullpath);
        return ResponseBuilder::generateError(500, server, request);
    }

    Logger::logFrom(LogLevel::INFO, "Post Handler",
                    "Successfully wrote URL-encoded form to: " + fullpath);
    return ResponseBuilder::generateSuccess(
        201, "<h1>Form Received. File " + filename + " created.</h1>", "text/html", request);
}

static HttpResponse handleRawBody(const HttpRequest& request, const Server& server,
                                  const std::string& fullpath, const std::string& filename) {
    std::ofstream out(fullpath, std::ios::binary);
    if (!out.is_open()) {
        Logger::logFrom(LogLevel::ERROR, "Post Handler",
                        "Failed to open file for writing: " + fullpath);
        return ResponseBuilder::generateError(500, server, request);
    }

    out << request.getBody();
    out.close();
    if (out.fail()) {
        Logger::logFrom(LogLevel::ERROR, "Post Handler",
                        "Failed to write or close file: " + fullpath);
        return ResponseBuilder::generateError(500, server, request);
    }

    Logger::logFrom(LogLevel::INFO, "Post Handler", "Successfully saved raw body to: " + fullpath);
    return ResponseBuilder::generateSuccess(
        201, "<html><body><h1>File " + filename + " created.</h1></body></html>", "text/html",
        request);
}

static std::string generateFilename() {
    return "upload_" + std::to_string(std::time(nullptr));
}

} // namespace

HttpResponse handlePost(const HttpRequest& request, const Server& server, const Location& loc) {
    if (request.getBody().empty()) {
        return ResponseBuilder::generateError(400, server, request);
    }

    if (request.getBody().size() > server.getClientMaxBodySize()) {
        return ResponseBuilder::generateError(413, server, request);
    }

    if (loc.getUploadStore().empty()) {
        return ResponseBuilder::generateError(403, server, request);
    }

    std::string candidate = resolvePhysicalPath(request, loc);

    if (candidate.empty()) {
        return ResponseBuilder::generateError(403, server, request);
    }

    std::filesystem::path candPath(candidate);
    if (std::filesystem::is_directory(candPath) || candidate.back() == '/') {
        std::string genName = generateFilename();
        candPath            = candPath / genName;
        candidate           = candPath.string();
    }

    std::filesystem::path fsCandidate(candidate);
    std::filesystem::path dirPath     = fsCandidate.parent_path();
    std::string           fullDirPath = dirPath.string();
    std::string           filename    = fsCandidate.filename().string();

    if (filename.find("..") != std::string::npos) {
        return ResponseBuilder::generateError(400, server, request);
    }

    if (isSymlink(candidate)) {
        return ResponseBuilder::generateError(403, server, request);
    }

    if (!mkdirRecursive(fullDirPath)) {
        return ResponseBuilder::generateError(500, server, request);
    }

    if (isFile(candidate)) {
        return ResponseBuilder::generateError(400, server, request);
    }

    const std::string contentType = request.getHeader("Content-Type");
    if (!contentType.empty() && contentType.find("multipart/form-data") != std::string::npos) {
        return handleMultipartForm(request, server, fullDirPath);
    }
    if (!contentType.empty() &&
        contentType.find("application/x-www-form-urlencoded") != std::string::npos) {
        return handleUrlEncodedForm(request, server, candidate, filename);
    }

    return handleRawBody(request, server, candidate, filename);
}
