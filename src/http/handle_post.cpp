/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_post.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/19 10:19:13 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/21 13:07:59 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handle_post.hpp"

bool parseMultipart(const std::string& body, const std::string& boundary,
                    std::string& filename, std::string& fileContent) {
    std::string delimiter = "--" + boundary;
    size_t pos = body.find(delimiter);
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
    fileContent = part.substr(headerEnd + 4); // after CRLFCRLF

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


std::string urlDecode(const std::string& encoded) {
    std::string decoded;
    decoded.reserve(encoded.size());  // optional: performance

    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%') {
            if (i + 2 < encoded.length()) {
                std::istringstream iss(encoded.substr(i + 1, 2));
                int hex = 0;
                if (iss >> std::hex >> hex) {
                    decoded += static_cast<char>(hex);
                    i += 2;
                } else {
                    // malformed % sequence, copy literally (we have return an error)
                    decoded += '%';
                }
            } else {
                // malformed % at end of string (we have return an error)
                decoded += '%';
            }
        } else if (encoded[i] == '+') {
            decoded += ' ';
        } else {
            decoded += encoded[i];
        }
    }

    return decoded;
}

std::map<std::string, std::string> parseUrlEncodedForm(const std::string& body) {
    std::map<std::string, std::string> form;
    std::istringstream ss(body);
    std::string pair;

    while (std::getline(ss, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = urlDecode(pair.substr(0, eq));
            std::string value = urlDecode(pair.substr(eq + 1));
            form[key] = value;
        }
    }

    return form;
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

    std::string relative = request.getPath().substr(loc.getPath().length());
    if (!relative.empty() && relative[0] == '/')
        relative = relative.substr(1);
    // check it in parser
    if (relative.find("..") != std::string::npos) {
        std::cerr << "[POST] Invalid relative: " << relative << std::endl;
        return ResponseBuilder::generateError(400, server, request);
    }
    /* std::cout << "[POST] Relative: " << relative << std::endl; */

    size_t      pos = relative.find_last_of('/');
    std::string relativeDir;
    std::string filename;
    if (pos == std::string::npos) {
        relativeDir = "";
        filename    = relative;
    } else {
        relativeDir = relative.substr(0, pos);
        filename    = relative.substr(pos + 1);
    }
    if (filename.empty()) {
        filename = "upload_" + std::to_string(std::time(nullptr))/*  + ".html" */;
    }

    std::string dirpath;
    if (loc.getUploadStore().empty()) {
        return ResponseBuilder::generateError(403, server, request);
    } else if (loc.getUploadStore()[0] == '/') {
        dirpath = loc.getUploadStore();
    } else {
        dirpath = joinPath(loc.getRoot(), loc.getUploadStore());
    }
    std::string fullDirPath = joinPath(dirpath, relativeDir);
    std::string fullpath    = joinPath(fullDirPath, filename);

    /* std::cout << "[POST] fullDirPath: " << fullDirPath << std::endl;
    std::cout << "[POST] dirpath: " << dirpath << std::endl;
    std::cout << "[POST] filename: {" << filename << "}" << std::endl;
    std::cout << "[POST] fullpath " << fullpath << std::endl; */

    if (!mkdirRecursive(fullDirPath)) {
        return ResponseBuilder::generateError(500, server, request);
    }

    if (fileExists(fullpath)) { // Think. We have to overwrite I guess.
        std::cerr << "[POST] File already exists: " << fullpath << std::endl;
        return ResponseBuilder::generateError(400, Server(), request);
    }
    std::ofstream file(fullpath.c_str());
    if (!file) {
        std::cerr << "[POST] Failed to open file: " << fullpath << std::endl;
        return ResponseBuilder::generateError(500, server, request);
    }

    std::string contentType = request.getHeader("Content-Type");

    if (!contentType.empty() && contentType.find("multipart/form-data") != std::string::npos) {
        size_t bpos = contentType.find("boundary=");
        if (bpos == std::string::npos) {
            return ResponseBuilder::generateError(400, server, request);
        }

        std::string boundary = contentType.substr(bpos + 9); // after "boundary="

        std::string filename, fileContent;
        if (!parseMultipart(request.getBody(), boundary, filename, fileContent)) {
            return ResponseBuilder::generateError(400, server, request);
        }

        if (filename.empty())
            filename = "upload_" + std::to_string(std::time(nullptr));

        std::string dirpath = joinPath(loc.getRoot(), loc.getUploadStore());
        std::string fullpath = joinPath(dirpath, filename);

        if (!mkdirRecursive(dirpath)) {
            return ResponseBuilder::generateError(500, server, request);
        }

        std::ofstream out(fullpath.c_str());
        if (!out)
            return ResponseBuilder::generateError(500, server, request);

        out << fileContent;
        out.close();

        if (out.fail())
            return ResponseBuilder::generateError(500, server, request);

        return ResponseBuilder::generateSuccess(201,
            "<html><body><h1>Uploaded: " + filename + "</h1></body></html>",
            "text/html", request);
    }

    if (!contentType.empty() && contentType.find("application/x-www-form-urlencoded") != std::string::npos) {
        std::map<std::string, std::string> form = parseUrlEncodedForm(request.getBody());
        if (form.empty()) {
            std::cerr << "[POST] Invalid or empty form data." << std::endl; // check nginx
            return ResponseBuilder::generateError(400, server, request);
        }
        std::string html = "<html><body><h1>Form Received</h1>";
        for (auto it = form.begin(); it != form.end(); ++it) {
            html += "<p><b>" + it->first + ":</b> " + it->second + "</p>";
        }
        html += "</body></html>";
        file << html;
         file.close();

        if (file.fail()) {
            std::cerr << "[POST] Failed to write or close file: " << fullpath << std::endl;
            return ResponseBuilder::generateError(500, server, request);
        }
        std::cout << "[POST] Form Received successfully: " << fullpath << std::endl;
        return ResponseBuilder::generateSuccess(201, "<h1>Form Received. File " + filename + " created.</h1>",
                                                "text/html", request);
    } else {
        file << request.getBody();
        file.close();

        if (file.fail()) {
            std::cerr << "[POST] Failed to write or close file: " << fullpath << std::endl;
            return ResponseBuilder::generateError(500, server, request);
        }
    }
    /*  HttpResponse response = ResponseBuilder::generateSuccess(201, body, "text/html", request);
     response.setHeader("Location", joinPath(request.getPath(), filename));  // Do we need it?

     return response; */

    std::cout << "[POST] File saved successfully: " << fullpath << std::endl;
    return ResponseBuilder::generateSuccess(201, "<html><body><h1>File " + filename + " created.</h1></body></html>",
                                            "text/html", request);
}
