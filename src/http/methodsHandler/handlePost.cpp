/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handlePost.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/19 10:19:13 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/06 21:31:31 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/responseBuilder.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/urlUtils.hpp"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace {

static bool parseMultipartForm(const std::string& body, const std::string& boundary,
                               std::string& filename, std::string& fileContent) {

    std::string partDelimiter  = "--" + boundary + "\r\n";
    std::string closeDelimiter = "--" + boundary + "--";

    size_t curPos = 0;
    while (true) {
        size_t start = body.find(partDelimiter, curPos);
        if (start == std::string::npos) {
            return false;
        }
        start += partDelimiter.size();

        size_t maybeClose = start - partDelimiter.size();
        if (body.compare(maybeClose, closeDelimiter.size(), closeDelimiter) == 0) {
            return false;
        }

        size_t nextPartPos = body.find(partDelimiter, start);
        size_t closePos    = body.find(closeDelimiter, start);
        size_t endPos;
        if (closePos == std::string::npos && nextPartPos == std::string::npos) {
            return false;
        } else if (closePos == std::string::npos) {
            endPos = nextPartPos;
        } else if (nextPartPos == std::string::npos) {
            endPos = closePos;
        } else {
            endPos = std::min(nextPartPos, closePos);
        }

        std::string part = body.substr(start, endPos - start);

        size_t headerEnd = part.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            return false;
        }
        std::string headersBlock = part.substr(0, headerEnd);
        std::string dataBlock    = part.substr(headerEnd + 4);

        size_t fnamePos = headersBlock.find("filename=\"");
        if (fnamePos == std::string::npos) {
            curPos = endPos;
            continue;
        }

        fnamePos += sizeof("filename=\"") - 1;
        size_t endQuote = headersBlock.find("\"", fnamePos);
        if (endQuote == std::string::npos) {
            return false;
        }
        filename = headersBlock.substr(fnamePos, endQuote - fnamePos);

        if (dataBlock.size() >= 2 && dataBlock.substr(dataBlock.size() - 2) == "\r\n") {
            fileContent = dataBlock.substr(0, dataBlock.size() - 2);
        } else {
            fileContent = dataBlock;
        }
        return true;
    }

    return false;
}

static HttpResponse handleMultipartForm(const HttpRequest& request, const Server& server,
                                        const std::string& fullDirPath) {
    std::string contentType = request.getHeader("Content-Type");
    size_t      bpos        = contentType.find("boundary=");
    if (bpos == std::string::npos) {
        return ResponseBuilder::generateError(400, server, request);
    }
    std::string boundary = contentType.substr(bpos + 9);

    std::string extractedFilename;
    std::string fileContent;
    if (!parseMultipartForm(request.getBody(), boundary, extractedFilename, fileContent)) {
        return ResponseBuilder::generateError(400, server, request);
    }

    if (extractedFilename.empty()) {
        extractedFilename = "upload_" + std::to_string(std::time(nullptr));
    }

    std::string fullpath = joinPath(fullDirPath, extractedFilename);

    std::ofstream out(fullpath, std::ios::binary);
    if (!out.is_open())
        return ResponseBuilder::generateError(500, server, request);

    out << fileContent;
    out.close();
    if (out.fail())
        return ResponseBuilder::generateError(500, server, request);

    return ResponseBuilder::generateSuccess(
        201, "<html><body><h1>Uploaded: " + extractedFilename + "</h1></body></html>", "text/html",
        request);
}

static HttpResponse handleUrlEncodedForm(const HttpRequest& request, const Server& server,
                                         const std::string& fullpath, const std::string& filename) {
    auto form = parseFormUrlEncoded(request.getBody());
    if (form.empty()) {
        return ResponseBuilder::generateError(400, server, request);
    }

    std::string html = "<html><body><h1>Form Received</h1>";
    for (auto& kv : form) {
        html += "<p><b>" + kv.first + ":</b> " + kv.second + "</p>";
    }
    html += "</body></html>";

    std::ofstream out(fullpath);
    if (!out.is_open())
        return ResponseBuilder::generateError(500, server, request);

    out << html;
    out.close();
    if (out.fail()) {
        return ResponseBuilder::generateError(500, server, request);
    }

    return ResponseBuilder::generateSuccess(
        201, "<h1>Form Received. File " + filename + " created.</h1>", "text/html", request);
}

static HttpResponse handleRawBody(const HttpRequest& request, const Server& server,
                                  const std::string& fullpath, const std::string& filename) {
    std::ofstream out(fullpath, std::ios::binary);
    if (!out.is_open())
        return ResponseBuilder::generateError(500, server, request);

    out << request.getBody();
    out.close();
    if (out.fail())
        return ResponseBuilder::generateError(500, server, request);

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
