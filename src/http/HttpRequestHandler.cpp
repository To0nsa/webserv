/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestHandler.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/12 23:13:23 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/16 11:57:33 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpRequestHandler.hpp"
#include "core/Location.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "http/handle_get.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>

HttpResponse handlePost(const HttpRequest&, const Server&, const Location&);
HttpResponse handleDelete(const HttpRequest&, const Server&, const Location&);
HttpResponse handleCgi(const HttpRequest&, const Server&, const Location&);

HttpResponse handleRequest(const HttpRequest& request, const Server& server) {
    const std::string& method = request.getMethod();
    const std::string& path   = request.getPath();

    // Find matching location
    const Location* matched = nullptr;
    for (const Location& loc : server.getLocations()) {
        if (loc.matchesPath(path)) {
            matched = &loc;
            break;
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

    return ResponseBuilder::generateError(501, server, request); // Not implemented
}

HttpResponse handlePost(const HttpRequest& request, const Server& server, const Location& loc) {
	std::cout << "{" << loc.getUploadStore() << "}" << std::endl;
    if (request.getBody().empty()) {
        return ResponseBuilder::generateError(400, server, request);
    }
    if (request.getBody().size() > server.getClientMaxBodySize()) {
        return ResponseBuilder::generateError(413, server, request);
    }
    if (loc.getUploadStore().empty()) {
        return ResponseBuilder::generateError(403, server, request);
    }
    std::string filepath = buildFilePath(request, loc);
    std::string filename = request.getPath().substr(request.getPath().find_last_of("/") + 1); // We need valid filename!!!
    /* (Reject empty filenames
    Reject things like ../somefile
    Limit file extensions) */
    std::string dirpath  = joinPath(loc.getRoot(), loc.getUploadStore());
    std::string fullpath = joinPath(dirpath, filename);
    if (mkdir(dirpath.c_str(), 0777) == -1 && errno != EEXIST) {
        return ResponseBuilder::generateError(500, server, request);
    }
    std::ofstream file(fullpath.c_str());
    if (!file) {
        return ResponseBuilder::generateError(500, server, request);
    }
    file << request.getBody();
    file.close();
    if (file.fail()) {
        return ResponseBuilder::generateError(500, server, request);
    }
    return ResponseBuilder::generateSuccess(201, "<h1>File " + filename +
                                            " created.</h1>", "text/html", request);
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

HttpResponse handleCgi(const HttpRequest& request, const Server& server, const Location& loc) {
    (void) loc;
    (void) server;
    return ResponseBuilder::generateSuccess(200, "<h1>Success</h1><p>OK</p>", "text/html", request);
}
