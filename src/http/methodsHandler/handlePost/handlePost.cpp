/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handlePost.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/19 10:19:13 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/09 12:33:38 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/methodsHandler.hpp"
#include "http/responseBuilder.hpp"
#include "utils/Logger.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/urlUtils.hpp"

#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
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
    for (auto& formField : form) {
        html += "<p><b>" + formField.first + ":</b> " + formField.second + "</p>";
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

    Logger::logFrom(LogLevel::INFO, "Post Handler", "Successfully saved file to: " + fullpath);
    return ResponseBuilder::generateSuccess(
        201, "<html><body><h1>File " + filename + " created.</h1></body></html>", "text/html",
        request);
}

/* static std::string generateFilename() {
    return "upload_" + std::to_string(std::time(nullptr));
} */

static std::atomic<uint64_t> uploadCounter{0};

static std::string generateFilename() {
    // 1) high-res time → nanoseconds since epoch
    auto     now = std::chrono::system_clock::now().time_since_epoch();
    uint64_t ns  = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    // 2) per-process counter to break ties within the same nanosecond
    uint64_t           seq = uploadCounter.fetch_add(1, std::memory_order_relaxed);
    std::ostringstream oss;
    oss << "upload_" << ns << "_" << seq;
    return oss.str();
}

} // namespace

static std::optional<HttpResponse>
validatePostRequest(HttpRequest const& request, Server const& server, Location const& location) {
    if (request.getBody().empty()) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "Empty body → rejecting POST for URI: " + request.getPath());
        return ResponseBuilder::generateError(400, server, request);
    }

    if (request.getBody().size() > server.getClientMaxBodySize()) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "Body size " + std::to_string(request.getBody().size()) +
                            " exceeds max client body size " +
                            std::to_string(server.getClientMaxBodySize()) +
                            " → rejecting POST for URI: " + request.getPath());
        return ResponseBuilder::generateError(413, server, request);
    }

    if (location.getUploadStore().empty()) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "No upload store configured → rejecting POST for URI: " +
                            request.getPath());
        return ResponseBuilder::generateError(403, server, request);
    }

    return std::nullopt;
}

static std::optional<HttpResponse>
preparePostTargetPath(HttpRequest const& request, Server const& server, Location const& location,
                      std::filesystem::path& outTargetPath, std::string& outTargetDirectory,
                      std::string& outTargetFilename) {
    // NGINX: any POST to "/file.ext/" MUST be 404 if "file.ext" exists in the root.

    // normalize URI and location prefix
    std::string reqPath = normalizePath(request.getPath());
    std::string locPref = normalizePath(location.getPath());
    // only if URI ends with '/'
    if (!reqPath.empty() && reqPath.back() == '/' && reqPath.rfind(locPref, 0) == 0) {
        // compute the root-based file path (ignore upload_store)
        std::string rel = reqPath.substr(locPref.size());
        while (!rel.empty() && rel.front() == '/')
            rel.erase(0, 1);
        std::string rootBase = normalizePath(location.getRoot());
        std::string rootFull = joinPath(rootBase, rel);
        // strip trailing slash if any
        if (!rootFull.empty() && rootFull.back() == '/')
            rootFull.pop_back();
        if (isFile(rootFull)) {
            Logger::logFrom(LogLevel::WARN, "Post Handler",
                            "Trailing slash on file → rejecting POST for URI: " +
                                request.getPath());
            return ResponseBuilder::generateError(404, server, request);
        }
    }
    std::string physicalPath = resolvePhysicalPath(request, location);

    if (physicalPath.empty()) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "Path empty → rejecting POST for URI: " + request.getPath());
        return ResponseBuilder::generateError(403, server, request);
    }

    std::filesystem::path candidatePath(physicalPath);

    // If it’s a directory (or ends with '/'), generate a filename
    if (std::filesystem::is_directory(candidatePath) || physicalPath.back() == '/') {
        std::string generatedFilename = generateFilename();
        candidatePath /= generatedFilename;
    }

    std::filesystem::path directoryPath = candidatePath.parent_path();
    outTargetDirectory                  = directoryPath.string();
    outTargetFilename                   = candidatePath.filename().string();

    if (outTargetFilename.find("..") != std::string::npos) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "Path traversal detected in filename → rejecting POST for URI: " +
                            request.getPath() + " filename: " + outTargetFilename);
        return ResponseBuilder::generateError(400, server, request);
    }

    if (isSymlink(candidatePath.string())) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "Target is a symlink → rejecting POST for URI: " + request.getPath() +
                            " path: " + candidatePath.string());
        return ResponseBuilder::generateError(403, server, request);
    }

    if (!mkdirRecursive(outTargetDirectory)) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "Failed to create directory: " + outTargetDirectory +
                            " → rejecting POST for URI: " + request.getPath());
        return ResponseBuilder::generateError(500, server, request);
    }

    if (isFile(candidatePath.string())) {
        Logger::logFrom(LogLevel::WARN, "Post Handler",
                        "File already exists → rejecting POST for URI: " + request.getPath() +
                            " path: " + candidatePath.string());
        return ResponseBuilder::generateError(400, server, request);
    }

    outTargetPath = candidatePath;
    return std::nullopt;
}

static HttpResponse dispatchPostByContentType(HttpRequest const& request, Server const& server,
                                              std::filesystem::path const& targetPath,
                                              std::string const&           targetDirectory,
                                              std::string const&           targetFilename) {
    std::string contentTypeHeader = request.getHeader("Content-Type");

    if (contentTypeHeader.find("multipart/form-data") != std::string::npos) {
        return handleMultipartForm(request, server, targetDirectory);
    }

    if (contentTypeHeader.find("application/x-www-form-urlencoded") != std::string::npos) {
        return handleUrlEncodedForm(request, server, targetPath.string(), targetFilename);
    }

    return handleRawBody(request, server, targetPath.string(), targetFilename);
}

HttpResponse handlePost(HttpRequest const& request, Server const& server,
                        Location const& location) {
    // 1) Preconditions
    std::optional<HttpResponse> maybeErrorResponse = validatePostRequest(request, server, location);
    if (maybeErrorResponse.has_value()) {
        return *maybeErrorResponse;
    }

    // 2) Filesystem path prep
    std::filesystem::path targetPath;
    std::string           targetDirectory;
    std::string           targetFilename;

    std::optional<HttpResponse> maybePreparationError = preparePostTargetPath(
        request, server, location, targetPath, targetDirectory, targetFilename);
    if (maybePreparationError.has_value()) {
        return *maybePreparationError;
    }

    // 3) Content-type dispatch
    return dispatchPostByContentType(request, server, targetPath, targetDirectory, targetFilename);
}
