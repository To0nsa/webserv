/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_post.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/19 10:19:13 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/30 19:59:40 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handle_post.hpp"

static bool parseMultipart(const std::string& body, const std::string& boundary,
                           std::string& filename, std::string& fileContent) {
    std::string delimiter = "--" + boundary;
    size_t      pos       = body.find(delimiter);
    if (pos == std::string::npos)
        return false;

    pos += delimiter.length() + 2; // Skip "\r\n"
    size_t end = body.find(delimiter + "--");
    if (end == std::string::npos)
        return false;

    std::string part = body.substr(pos, end - pos);

    // Separate headers from content
    size_t headerEnd = part.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
        return false;

    std::string headers = part.substr(0, headerEnd);
    fileContent         = part.substr(headerEnd + 4); // after CRLFCRLF

    // Extract filename
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

            std::string hexStr = encoded.substr(i + 1, 2);
            int         hex    = std::stoi(hexStr, 0, 16);
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
                form[key]         = value;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[POST] Malformed URL-encoded data: " << e.what() << std::endl;
        form.clear();
    }

    return form;
}

static HttpResponse handle_multipart_form(const HttpRequest& request, const Server& server,
                                          const std::string& fullDirPath) {
    std::string contentType = request.getHeader("Content-Type");
    size_t      bpos        = contentType.find("boundary=");
    if (bpos == std::string::npos) {
        std::cerr << "[POST] Invalid Content-Type: " << contentType << std::endl;
        return ResponseBuilder::generateError(400, server, request);
    }
    std::string boundary = contentType.substr(bpos + 9);

    std::string extractedFilename, fileContent;
    if (!parseMultipart(request.getBody(), boundary, extractedFilename, fileContent)) {
        std::cerr << "[POST] Failed to parse multipart form data." << std::endl;
        return ResponseBuilder::generateError(400, server, request);
    }
    if (extractedFilename.empty())
        extractedFilename = "upload_" + std::to_string(std::time(NULL));
    std::string fullpath = joinPath(fullDirPath, extractedFilename);

    std::cout << "[POST] filename: {" << extractedFilename << "}" << std::endl;
    std::cout << "[POST] fullpath: {" << fullpath << "}" << std::endl;

    std::ofstream out(fullpath.c_str());
    if (!out)
        return ResponseBuilder::generateError(500, server, request);
    out << fileContent;
    out.close();
    if (out.fail())
        return ResponseBuilder::generateError(500, server, request);
    return ResponseBuilder::generateSuccess(
        201, "<html><body><h1>Uploaded: " + extractedFilename + "</h1></body></html>", "text/html",
        request);
}

static HttpResponse handle_url_encoded_form(const HttpRequest& request, const Server& server,
                                            const std::string& fullpath,
                                            const std::string& filename) {
    std::map<std::string, std::string> form = parseUrlEncodedForm(request.getBody());
    if (form.empty()) {
        std::cerr << "[POST] Invalid or empty form data." << std::endl; // check nginx
        return ResponseBuilder::generateError(400, server, request);
    }
    std::string html = "<html><body><h1>Form Received</h1>";
    for (std::map<std::string, std::string>::iterator it = form.begin(); it != form.end(); ++it)
        html += "<p><b>" + it->first + ":</b>" + it->second + "</p>";
    html += "</body></html>";

    std::cout << "[POST] filename: {" << filename << "}" << std::endl;
    std::cout << "[POST] fullpath: {" << fullpath << "}" << std::endl;

    std::ofstream out(fullpath.c_str());
    if (!out)
        return ResponseBuilder::generateError(500, server, request);
    out << html;
    out.close();
    if (out.fail()) {
        std::cerr << "[POST] Failed to write or close file: " << fullpath << std::endl;
        return ResponseBuilder::generateError(500, server, request);
    }
    std::cout << "[POST] Form Received successfully: " << fullpath << std::endl;
    return ResponseBuilder::generateSuccess(
        201, "<h1>Form Received. File " + filename + " created.</h1>", "text/html", request);
}

static HttpResponse handle_raw_body(const HttpRequest& request, const Server& server,
                                    const std::string& fullpath, const std::string& filename) {
    std::cout << "[POST] filename: {" << filename << "}" << std::endl;
    std::cout << "[POST] fullpath: {" << fullpath << "}" << std::endl;
    std::ofstream out(fullpath.c_str());
    if (!out)
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

static std::string resolveRelativePath(const HttpRequest& request, const Location& loc) {
    std::string locPath  = normalizePath(loc.getPath());
    std::string reqPath  = normalizePath(request.getPath());
    std::string relative = reqPath.substr(locPath.length());
    if (!relative.empty() && relative[0] == '/')
        relative = relative.substr(1);
    return relative;
}

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

static std::pair<std::string, std::string>
resolveUploadPaths(const Location& loc, const std::string& relative, const std::string& filename) {
    std::string uploadStore = normalizePath(loc.getUploadStore());
    std::string root        = normalizePath(loc.getRoot());

    std::string relativeDir;
    size_t      pos = relative.find_last_of('/');
    if (pos != std::string::npos)
        relativeDir = relative.substr(0, pos);

    std::string dirpath     = (uploadStore[0] == '/') ? uploadStore : joinPath(root, uploadStore);
    std::string fullDirPath = joinPath(dirpath, relativeDir);
    std::string fullpath    = joinPath(fullDirPath, filename);
    return std::make_pair(fullDirPath, fullpath);
}

HttpResponse handlePost(const HttpRequest& request, const Server& server, const Location& loc) {
    std::cout << "[POST] Upload store: {" << loc.getUploadStore() << "}" << std::endl;

    if (request.getBody().empty()) {
        std::cout << "[POST] Body is empty — returning 400" << std::endl;
        return ResponseBuilder::generateError(400, server, request);
    }

    if (request.getBody().size() > server.getClientMaxBodySize()) {
        std::cout << "[POST] Body too large (" << request.getBody().size()
                  << " bytes) — returning 413" << std::endl;
        return ResponseBuilder::generateError(413, server, request);
    }

    if (loc.getUploadStore().empty()) {
        std::cout << "[POST] Upload store is not configured — returning 403" << std::endl;
        return ResponseBuilder::generateError(403, server, request);
    }

    std::string relative = resolveRelativePath(request, loc);
    if (relative.find("..") != std::string::npos) {
        std::cerr << "[POST] Invalid relative: " << relative << std::endl;
        return ResponseBuilder::generateError(400, server, request);
    }
    /* std::cout << "[POST] Relative: " << relative << std::endl; */

    std::string filename = extractFilename(relative);
    std::string fullDirPath, fullpath;
    std::tie(fullDirPath, fullpath) = resolveUploadPaths(loc, relative, filename);

    if (!mkdirRecursive(fullDirPath)) {
        return ResponseBuilder::generateError(500, server, request);
    }

    if (isFile(fullpath)) { // Think. We have to overwrite I guess.
        std::cerr << "[POST] File already exists: " << fullpath << std::endl;
        return ResponseBuilder::generateError(400, Server(), request);
    }

    std::string contentType = request.getHeader("Content-Type");
    if (!contentType.empty() && contentType.find("multipart/form-data") != std::string::npos)
        return handle_multipart_form(request, server, fullDirPath);
    if (!contentType.empty() &&
        contentType.find("application/x-www-form-urlencoded") != std::string::npos)
        return handle_url_encoded_form(request, server, fullpath, filename);
    return handle_raw_body(request, server, fullpath, filename);
}
