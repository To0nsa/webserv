/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestHandler.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/12 23:13:23 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/18 10:42:50 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpRequestHandler.hpp"
#include "core/Location.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "http/handle_get.hpp"
#include "http/handleCgi.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>

HttpResponse handlePost(const HttpRequest&, const Server&, const Location&);
HttpResponse handleDelete(const HttpRequest&, const Server&, const Location&);

HttpResponse handleRequest(const HttpRequest& request, const Server& server) {
    const std::string& method = request.getMethod();
    const std::string& path   = request.getPath();

    // Find matching location
    const Location* matched     = nullptr;
    size_t          maxMatchLen = 0;

    std::cout << "Requested path: {" << path << "}" << std::endl;
    std::cout << "Requested method: {" << method << "}" << std::endl;
    for (const Location& loc : server.getLocations()) {
        const std::string& locPath = loc.getPath();
        std::cout << "Upload store for {" << loc.getPath() << "} : {" << loc.getUploadStore() << "}" << std::endl;
        if (path.compare(0, locPath.size(), locPath) == 0 && locPath.size() > maxMatchLen) {
            matched     = &loc;
            std::cout << "Upload store MATCHED for {" << loc.getPath() << "} : {" << loc.getUploadStore() << "}" << std::endl;
            maxMatchLen = locPath.size();
        }
    }

    if (!matched) {
        return ResponseBuilder::generateError(404, server, request);
    }

    const Location& location = *matched;

    // Handle HTTP redirection
    if (location.hasRedirect()) {
        return ResponseBuilder::generateRedirect(location.getReturnCode(), location.getRedirect(),
                                                 request);
    }

    static const std::set<std::string> implemented = {"GET", "POST", "DELETE"};
    if (implemented.find(method) == implemented.end()) {
        return ResponseBuilder::generateError(501, server, request);
    }

    // Method not allowed
    if (!location.isMethodAllowed(method)) {
        return ResponseBuilder::generateError(405, server, request);
    }

    // CGI detection
    if (location.isCgiRequest(path)) {
        return handleCgi(request, server, location);
    }

    // Delegate based on method
    if (method == "GET") {
        return handleGet(request, server, location);
    } else if (method == "POST") {
        return handlePost(request, server, location);
    } else if (method == "DELETE") {
        return handleDelete(request, server, location);
    }

    return ResponseBuilder::generateError(500, server, request);
}

#include <string.h>

HttpResponse handlePost(const HttpRequest& request, const Server& server, const Location& loc) {
    std::cout << "[POST] Upload store: {" << loc.getUploadStore() << "}" << std::endl;

    if (request.getBody().empty()) {
        std::cout << "[POST] Body is empty — returning 400" << std::endl;
        return ResponseBuilder::generateError(400, server, request);
    }

    if (request.getBody().size() > server.getClientMaxBodySize()) {
        std::cout << "[POST] Body too large (" << request.getBody().size() << " bytes) — returning 413" << std::endl;
        return ResponseBuilder::generateError(413, server, request);
    }

    if (loc.getUploadStore().empty()) {
        std::cout << "[POST] Upload store is not configured — returning 403" << std::endl;
        return ResponseBuilder::generateError(403, server, request);
    }

    std::string filename = request.getPath().substr(request.getPath().find_last_of("/") + 1);
    std::string dirpath;
    if (loc.getUploadStore().empty()) {
        return ResponseBuilder::generateError(403, server, request);
    } else if (loc.getUploadStore()[0] == '/') {
        dirpath = loc.getUploadStore();
    } else {
        dirpath = joinPath(loc.getRoot(), loc.getUploadStore());
    }
    std::string fullpath = joinPath(dirpath, filename);

    std::cout << "[POST] Upload directory: " << dirpath << std::endl;
    std::cout << "[POST] Target filename: " << filename << std::endl;
    std::cout << "[POST] Full file path: " << fullpath << std::endl;

    if (mkdir(dirpath.c_str(), 0777) == -1 && errno != EEXIST) {
        std::cerr << "[POST] Failed to create directory: " << dirpath << " — errno: " << strerror(errno) << std::endl;
        return ResponseBuilder::generateError(500, server, request);
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

    std::cout << "[POST] File saved successfully: " << fullpath << std::endl;
    return ResponseBuilder::generateSuccess(201, "<h1>File " + filename + " created.</h1>", "text/html", request);
}


HttpResponse handleDelete(const HttpRequest& request, const Server& server, const Location& loc) {
    // Build full file path
    std::string filepath = buildFilePath(request, loc);

    // Check if file exists and delete
    struct stat s;
    if (stat(filepath.c_str(), &s) != 0)
        return ResponseBuilder::generateError(404, server, request);
    if (!S_ISREG(s.st_mode))
        return ResponseBuilder::generateError(403, server, request);
    if (unlink(filepath.c_str()) != 0)
        return ResponseBuilder::generateError(500, server, request);
    std::string filename = request.getPath().substr(request.getPath().find_last_of("/") + 1);
    return ResponseBuilder::generateSuccess(200, "<h1>File " + filename + " deleted.</h1>",
                                            "text/html", request);
}
