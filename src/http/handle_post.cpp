/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_post.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/19 10:19:13 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/31 18:34:57 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handle_post.hpp"
#include "utils/filesystemUtils.hpp"

#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

// Parse a single-part file upload; extracts filename and content.
static bool parseMultipart(const std::string& body, const std::string& boundary,
                           std::string& filename, std::string& fileContent) {
    std::string delimiter = "--" + boundary;
    size_t      pos       = body.find(delimiter);
    if (pos == std::string::npos)
        return false;

    pos += delimiter.length() + 2; // skip "\r\n"
    size_t end = body.find(delimiter + "--");
    if (end == std::string::npos)
        return false;

    std::string part = body.substr(pos, end - pos);

    // Separate headers from content.
    size_t headerEnd = part.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
        return false;

    std::string headers = part.substr(0, headerEnd);
    fileContent         = part.substr(headerEnd + 4); // after "\r\n\r\n"

    // Extract filename="..."
    size_t fnamePos = headers.find("filename=\"");
    if (fnamePos == std::string::npos)
        return false;

    fnamePos += 10; // strlen("filename=\"")
    size_t endQuote = headers.find("\"", fnamePos);
    if (endQuote == std::string::npos)
        return false;

    filename = headers.substr(fnamePos, endQuote - fnamePos);
    return true;
}

// Decode percent-encoded data (e.g. "foo%20bar").
static std::string urlDecode(const std::string& encoded) {
    std::string decoded;
    decoded.reserve(encoded.size());

    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%') {
            if (i + 2 >= encoded.length())
                throw std::runtime_error("Incomplete percent-encoding at end of string");

            char hex1 = encoded[i + 1];
            char hex2 = encoded[i + 2];
            if (!isxdigit(hex1) || !isxdigit(hex2))
                throw std::runtime_error("Invalid hex in percent-encoding");

            int hex = std::stoi(encoded.substr(i + 1, 2), nullptr, 16);
            decoded += static_cast<char>(hex);
            i += 2;
        } else if (encoded[i] == '+') {
            decoded += ' ';
        } else {
            decoded += encoded[i];
        }
    }
    return decoded;
}

// Parse "application/x-www-form-urlencoded" body into key→value map.
static std::map<std::string, std::string> parseUrlEncodedForm(const std::string& body) {
    std::map<std::string, std::string> form;
    std::istringstream                 ss(body);
    std::string                        pair;

    try {
        while (std::getline(ss, pair, '&')) {
            size_t eq = pair.find('=');
            if (eq != std::string::npos) {
                std::string key   = urlDecode(pair.substr(0, eq));
                std::string value = urlDecode(pair.substr(eq + 1));
                form.emplace(std::move(key), std::move(value));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[POST] Malformed URL-encoded data: " << e.what() << std::endl;
        form.clear();
    }

    return form;
}

// Handle multipart/form-data (file upload).
static HttpResponse handle_multipart_form(const HttpRequest& request, const Server& server,
                                          const std::string& fullDirPath) {
    std::string contentType = request.getHeader("Content-Type");
    size_t      bpos        = contentType.find("boundary=");
    if (bpos == std::string::npos) {
        std::cerr << "[POST] Invalid Content-Type: " << contentType << std::endl;
        return ResponseBuilder::generateError(400, server, request);
    }
    std::string boundary = contentType.substr(bpos + 9);

    std::string extractedFilename;
    std::string fileContent;
    if (!parseMultipart(request.getBody(), boundary, extractedFilename, fileContent)) {
        std::cerr << "[POST] Failed to parse multipart form data." << std::endl;
        return ResponseBuilder::generateError(400, server, request);
    }

    if (extractedFilename.empty()) {
        extractedFilename = "upload_" + std::to_string(std::time(nullptr));
    }

    std::string fullpath = joinPath(fullDirPath, extractedFilename);
    std::cout << "[POST] filename: {" << extractedFilename << "}" << std::endl;
    std::cout << "[POST] fullpath: {" << fullpath << "}" << std::endl;

    // Write file content to disk.
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

// Handle application/x-www-form-urlencoded.
static HttpResponse handle_url_encoded_form(const HttpRequest& request, const Server& server,
                                            const std::string& fullpath,
                                            const std::string& filename) {
    auto form = parseUrlEncodedForm(request.getBody());
    if (form.empty()) {
        std::cerr << "[POST] Invalid or empty form data." << std::endl;
        return ResponseBuilder::generateError(400, server, request);
    }

    // Build a simple HTML representation of the form data.
    std::string html = "<html><body><h1>Form Received</h1>";
    for (auto& kv : form) {
        html += "<p><b>" + kv.first + ":</b> " + kv.second + "</p>";
    }
    html += "</body></html>";

    std::cout << "[POST] filename: {" << filename << "}" << std::endl;
    std::cout << "[POST] fullpath: {" << fullpath << "}" << std::endl;

    // Write HTML to disk.
    std::ofstream out(fullpath);
    if (!out.is_open())
        return ResponseBuilder::generateError(500, server, request);

    out << html;
    out.close();
    if (out.fail()) {
        std::cerr << "[POST] Failed to write or close file: " << fullpath << std::endl;
        return ResponseBuilder::generateError(500, server, request);
    }

    std::cout << "[POST] Form received successfully: " << fullpath << std::endl;
    return ResponseBuilder::generateSuccess(
        201, "<h1>Form Received. File " + filename + " created.</h1>", "text/html", request);
}

// Handle raw body → write verbatim to disk.
static HttpResponse handle_raw_body(const HttpRequest& request, const Server& server,
                                    const std::string& fullpath, const std::string& filename) {
    std::cout << "[POST] filename: {" << filename << "}" << std::endl;
    std::cout << "[POST] fullpath: {" << fullpath << "}" << std::endl;

    std::ofstream out(fullpath, std::ios::binary);
    if (!out.is_open())
        return ResponseBuilder::generateError(500, server, request);

    out << request.getBody();
    out.close();
    if (out.fail())
        return ResponseBuilder::generateError(500, server, request);

    std::cout << "[POST] File saved successfully: " << fullpath << std::endl;
    return ResponseBuilder::generateSuccess(
        201, "<html><body><h1>File " + filename + " created.</h1></body></html>", "text/html",
        request);
}

// Given a request path and location prefix, produce “relative” subpath.
static std::string resolveRelativePath(const HttpRequest& request, const Location& loc) {
    std::string locPath = normalizePath(loc.getPath());
    std::string reqPath = normalizePath(request.getPath());
    std::string rel     = reqPath.substr(locPath.length());
    if (!rel.empty() && rel[0] == '/')
        rel = rel.substr(1);
    return rel;
}

// From a “relative” path, extract just the filename (or generate one).
static std::string extractFilename(const std::string& relative) {
    size_t pos = relative.find_last_of('/');
    if (pos == std::string::npos) {
        if (relative.empty())
            return "upload_" + std::to_string(std::time(nullptr));
        return relative;
    }

    std::string filename = relative.substr(pos + 1);
    if (filename.empty())
        filename = "upload_" + std::to_string(std::time(nullptr));
    return filename;
}

// Based on upload_store + root, return (directory, fullpath).
static std::pair<std::string, std::string>
resolveUploadPaths(const Location& loc, const std::string& relative, const std::string& filename) {
    std::string uploadStore = normalizePath(loc.getUploadStore());
    std::string root        = normalizePath(loc.getRoot());

    // Determine any subdirectory under uploadStore, if “relative” contains “/”
    std::string relativeDir;
    size_t      pos = relative.find_last_of('/');
    if (pos != std::string::npos)
        relativeDir = relative.substr(0, pos);

    // If uploadStore is not absolute, interpret relative to loc.getRoot()
    std::string dirpath = (uploadStore.front() == '/') ? uploadStore : joinPath(root, uploadStore);
    std::string fullDirPath = joinPath(dirpath, relativeDir);
    std::string fullpath    = joinPath(fullDirPath, filename);
    return {fullDirPath, fullpath};
}

HttpResponse handlePost(const HttpRequest& request, const Server& server, const Location& loc) {
    std::cout << "[POST] Upload store: {" << loc.getUploadStore() << "}" << std::endl;

    // 1) Empty body → 400
    if (request.getBody().empty()) {
        std::cout << "[POST] Body is empty — returning 400" << std::endl;
        return ResponseBuilder::generateError(400, server, request);
    }

    // 2) Body too large → 413
    if (request.getBody().size() > server.getClientMaxBodySize()) {
        std::cout << "[POST] Body too large (" << request.getBody().size()
                  << " bytes) — returning 413" << std::endl;
        return ResponseBuilder::generateError(413, server, request);
    }

    // 3) No upload_store configured → 403
    if (loc.getUploadStore().empty()) {
        std::cout << "[POST] Upload store is not configured — returning 403" << std::endl;
        return ResponseBuilder::generateError(403, server, request);
    }

    // 4) Prevent directory traversal
    std::string relative = resolveRelativePath(request, loc);
    if (relative.find("..") != std::string::npos) {
        std::cerr << "[POST] Invalid relative path: " << relative << std::endl;
        return ResponseBuilder::generateError(400, server, request);
    }

    // 5) Determine filename and target paths
    std::string filename = extractFilename(relative);
    std::string fullDirPath, fullpath;
    std::tie(fullDirPath, fullpath) = resolveUploadPaths(loc, relative, filename);

    // 6) If the *exact* target already exists as a symlink, reject with 403
    if (isSymlink(fullpath)) {
        std::cerr << "[POST] Refusing to POST to symlink: " << fullpath << std::endl;
        return ResponseBuilder::generateError(403, server, request);
    }

    // 7) Ensure parent directory exists (mkdir -p semantics)
    if (!mkdirRecursive(fullDirPath)) {
        std::cerr << "[POST] Failed to create directory: " << fullDirPath << std::endl;
        return ResponseBuilder::generateError(500, server, request);
    }

    // 8) If a real file already exists, refuse (avoid overwrite)
    if (isFile(fullpath)) {
        std::cerr << "[POST] File already exists: " << fullpath << std::endl;
        return ResponseBuilder::generateError(400, server, request);
    }

    // 9) Branch based on Content-Type
    std::string contentType = request.getHeader("Content-Type");
    if (!contentType.empty() && contentType.find("multipart/form-data") != std::string::npos) {
        return handle_multipart_form(request, server, fullDirPath);
    }
    if (!contentType.empty() &&
        contentType.find("application/x-www-form-urlencoded") != std::string::npos) {
        return handle_url_encoded_form(request, server, fullpath, filename);
    }

    // 10) Otherwise, treat as raw body
    return handle_raw_body(request, server, fullpath, filename);
}
