/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handlePost.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/19 10:19:13 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/06 11:47:43 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/responseBuilder.hpp"
#include "utils/filesystemUtils.hpp"

#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

static bool parseMultipart(const std::string& body, const std::string& boundary,
                           std::string& filename, std::string& fileContent) {
    // Build the two important markers:
    //   "--<boundary>\r\n"      ← start of each part
    //   "--<boundary>--"        ← final closing boundary
    std::string partDelimiter  = "--" + boundary + "\r\n";
    std::string closeDelimiter = "--" + boundary + "--";

    // 1) Split body on "—boundary\r\n".  We ignore any leading data before the first boundary.
    //    Using std::string::find in a loop is simpler than a full split, so we'll do that.
    size_t curPos = 0;
    while (true) {
        // Find the next part delimiter
        size_t start = body.find(partDelimiter, curPos);
        if (start == std::string::npos) {
            // No more parts or malformed (no boundary at all)
            return false;
        }
        start += partDelimiter.size(); // move to just after "--<boundary>\r\n"

        // Check if this is actually the closing boundary (i.e. --boundary--<maybe CRLF>).
        // If what's immediately after start-offset is the closing delimiter, we’re done.
        // (Alternatively, some clients might send "--<boundary>--" on its own line with no trailing
        // "\r\n".)
        size_t maybeClose = start - partDelimiter.size(); // position of "--boundary"
        if (body.compare(maybeClose, closeDelimiter.size(), closeDelimiter) == 0) {
            // we've hit the terminating "--<boundary>--".  No file found.
            return false;
        }

        // 2) We found a valid part; now find where it ends.  That is either
        //    the next occurrence of "--<boundary>\r\n" or the final "--<boundary>--".
        size_t nextPartPos = body.find(partDelimiter, start);
        size_t closePos    = body.find(closeDelimiter, start);
        size_t endPos;
        if (closePos == std::string::npos && nextPartPos == std::string::npos) {
            // malformed/missing closing boundary
            return false;
        } else if (closePos == std::string::npos) {
            endPos = nextPartPos;
        } else if (nextPartPos == std::string::npos) {
            endPos = closePos;
        } else {
            endPos = std::min(nextPartPos, closePos);
        }

        // Extract this part's raw bytes (from start up to endPos, excluding trailing "\r\n")
        std::string part = body.substr(start, endPos - start);

        // 3) Split headers vs. content by the first "\r\n\r\n"
        size_t headerEnd = part.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            // Malformed part (no header-body separator)
            return false;
        }
        std::string headersBlock = part.substr(0, headerEnd);
        std::string dataBlock    = part.substr(headerEnd + 4); // everything after "\r\n\r\n"

        // 4) Look for filename="..." in the headers
        size_t fnamePos = headersBlock.find("filename=\"");
        if (fnamePos == std::string::npos) {
            // This part is a plain form‐field; skip it and keep searching
            curPos = endPos;
            continue;
        }

        // 5) Extract the filename between the quotes
        fnamePos += strlen("filename=\""); // move to first char of filename
        size_t endQuote = headersBlock.find("\"", fnamePos);
        if (endQuote == std::string::npos) {
            // Malformed filename header
            return false;
        }
        filename = headersBlock.substr(fnamePos, endQuote - fnamePos);

        // 6) The remainder of dataBlock is the file’s content.  Copy it out verbatim.
        //    We should strip off a trailing "\r\n" if present (some clients leave a CRLF before the
        //    next boundary).
        if (dataBlock.size() >= 2 && dataBlock.substr(dataBlock.size() - 2) == "\r\n") {
            fileContent = dataBlock.substr(0, dataBlock.size() - 2);
        } else {
            fileContent = dataBlock;
        }
        return true; // success: we found a file part
    }

    // unreachable
    return false;
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
