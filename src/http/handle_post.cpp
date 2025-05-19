/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_post.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/19 10:19:13 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/19 10:22:00 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handle_post.hpp"

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
    std::cout << "[POST] Relative: " << relative << std::endl;

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
        filename = "upload_" + std::to_string(std::time(nullptr)) + ".txt";
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

    std::cout << "[POST] fullDirPath: " << fullDirPath << std::endl;
    std::cout << "[POST] dirpath: " << dirpath << std::endl;
    std::cout << "[POST] filename: {" << filename << "}" << std::endl;
    std::cout << "[POST] fullpath " << fullpath << std::endl;

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

    file << request.getBody();
    file.close();

    if (file.fail()) {
        std::cerr << "[POST] Failed to write or close file: " << fullpath << std::endl;
        return ResponseBuilder::generateError(500, server, request);
    }
    /*  HttpResponse response = ResponseBuilder::generateSuccess(201, body, "text/html", request);
     response.setHeader("Location", joinPath(request.getPath(), filename));  // Do we need it?

     return response; */

    std::cout << "[POST] File saved successfully: " << fullpath << std::endl;
    return ResponseBuilder::generateSuccess(201, "<h1>File " + filename + " created.</h1>",
                                            "text/html", request);
}
