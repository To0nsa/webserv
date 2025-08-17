/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleDelete.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/21 15:06:07 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/17 11:25:09 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/Location.hpp"
#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/responseBuilder.hpp"
#include "utils/Logger.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/htmlUtils.hpp"
#include "utils/urlUtils.hpp"

#include <filesystem>
#include <sstream>
#include <string>
#include <sys/stat.h>

namespace {

bool unlinkFile(const std::string& filepath, const HttpRequest& req, const Server& server,
                HttpResponse& outError) {
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        Logger::logFrom(LogLevel::WARN, "Delete Handler",
                        "File not found → rejecting DELETE: " + filepath);
        outError = ResponseBuilder::generateError(404, server, req);
        return false;
    }
    if (S_ISDIR(st.st_mode)) {
        Logger::logFrom(LogLevel::WARN, "Delete Handler",
                        "Target is a directory → rejecting DELETE: " + filepath);
        outError = ResponseBuilder::generateError(403, server, req);
        return false;
    }
    if (!S_ISREG(st.st_mode)) {
        Logger::logFrom(LogLevel::WARN, "Delete Handler",
                        "Target is not a regular file → rejecting DELETE: " + filepath);
        outError = ResponseBuilder::generateError(403, server, req);
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::remove(filepath, ec)) {
        if (ec.value() == EACCES || ec.value() == EPERM) {
            Logger::logFrom(LogLevel::WARN, "Delete Handler",
                            "Permission denied when deleting: " + filepath + " (" + ec.message() +
                                ")");
            outError = ResponseBuilder::generateError(403, server, req);
        } else if (ec.value() == ENOENT) {
            Logger::logFrom(LogLevel::WARN, "Delete Handler",
                            "File disappeared before deletion: " + filepath);
            outError = ResponseBuilder::generateError(404, server, req);
        } else {
            Logger::logFrom(LogLevel::WARN, "Delete Handler",
                            "Unexpected error deleting file: " + filepath + " (" + ec.message() +
                                ")");
            outError = ResponseBuilder::generateError(500, server, req);
        }
        return false;
    }

    return true;
}

std::string generateDeleteHtml(const std::string& filename) {
    std::stringstream html;
    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Deleted: )"
         << htmlEscape(filename) << R"(</title>
</head>
<body>
  <div class="container">
    <h1>File )"
         << htmlEscape(filename) << R"( deleted.</h1>
    <p>The requested file has been successfully removed.</p>
  </div>
</body>
</html>)";
    return html.str();
}

} // namespace

HttpResponse handleDelete(const HttpRequest& req, const Server& server, const Location& loc) {
    std::string path = resolvePhysicalPath(req, loc);
    if (path.empty()) {
        Logger::logFrom(LogLevel::WARN, "Delete Handler",
                        "Path is empty → rejecting DELETE for URI: " + req.getPath());
        return ResponseBuilder::generateError(403, server, req);
    }

    if (isSymlink(path)) {
        Logger::logFrom(LogLevel::WARN, "Delete Handler",
                        "Target is a symlink → rejecting DELETE for URI: " + req.getPath());
        return ResponseBuilder::generateError(403, server, req);
    }

    if (req.getPath().back() == '/') {
        Logger::logFrom(LogLevel::WARN, "Delete Handler",
                        "Request URI ends with '/' → rejecting DELETE for directory-like path: " +
                            req.getPath());
        return ResponseBuilder::generateError(403, server, req);
    }

    HttpResponse errResp;
    if (!unlinkFile(path, req, server, errResp)) {
        return errResp;
    }

    std::string rawName  = extractFilenameFromUri(req.getPath());
    std::string safeName = htmlEscape(rawName);
    std::string body     = generateDeleteHtml(safeName);
    Logger::logFrom(LogLevel::INFO, "Delete Handler",
                    "Successfully deleted “" + safeName + "” → sending HTML confirmation");
    return ResponseBuilder::generateSuccess(200, body, "text/html", req);
}
