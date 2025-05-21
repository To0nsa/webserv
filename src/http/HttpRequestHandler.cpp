/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestHandler.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/12 23:13:23 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/21 13:06:52 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpRequestHandler.hpp"
#include "core/Location.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "http/handleCgi.hpp"
#include "http/handle_get.hpp"
#include "http/handle_post.hpp"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

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
        const std::string& locPath = normalizePath(loc.getPath());
        /* std::cout << "Upload store for {" << loc.getPath() << "} : {" << loc.getUploadStore() << "}"
                  << std::endl; */
        if (path.compare(0, locPath.size(), locPath) == 0 && locPath.size() > maxMatchLen) {
            matched = &loc;
            std::cout << "Upload store MATCHED for {" << loc.getPath() << "} : {"
                      << loc.getUploadStore() << "}" << std::endl;
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
        return ResponseBuilder::generateError(405, server, request); /* return ResponseBuilder::generateError(501, server, request); */
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
