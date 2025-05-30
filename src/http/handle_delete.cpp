/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_delete.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/21 15:06:07 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/30 19:59:45 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/* HttpResponse handleDelete(const HttpRequest& request, const Server& server, const Location& loc)
{
    // Build full file path
    std::string filepath = buildFilePath(request, loc);
    std::cout << "Resolved file path: " << filepath << std::endl;

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
} */

#include "http/handle_delete.hpp"
#include "utils/filesystemUtils.hpp"
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

HttpResponse handleDelete(const HttpRequest& request, const Server& server, const Location& loc) {

    // Normalize both the request path and the location prefix
    std::string requestPath = normalizePath(request.getPath());
    std::string locPrefix   = normalizePath(loc.getPath());

    // Decide which physical directory to delete from
    std::string filepath;
    if (loc.isUploadEnabled() && requestPath.rfind(locPrefix, 0) == 0) {
        // — Strip the location prefix from the request
        std::string relative = requestPath.substr(locPrefix.size());
        // — Remove any leading slashes
        while (!relative.empty() && relative.front() == '/')
            relative.erase(0, 1);

        // — Figure out the upload_store base
        std::string uploadRoot = normalizePath(loc.getUploadStore());
        // If upload_store was given as a relative path, interpret it under the normal root
        if (!uploadRoot.empty() && uploadRoot.front() != '/')
            uploadRoot = joinPath(normalizePath(loc.getRoot()), uploadRoot);

        // — Join into a full path under upload_store
        filepath = joinPath(uploadRoot, relative);
    } else {
        // Static content (or DELETE on a non-upload location)
        filepath = buildFilePath(request, loc);
        // return ResponseBuilder::generateError(403, server, request);
    }

    std::cout << "Resolved file path: " << filepath << std::endl;

    // 1) Must exist
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0)
        return ResponseBuilder::generateError(404, server, request);

    // 2) Must be a regular file
    if (!S_ISREG(st.st_mode)) {
        return ResponseBuilder::generateError(403, server, request);
    }

    if (request.getPath().back() == '/' && S_ISREG(st.st_mode)) {
        return ResponseBuilder::generateError(404, server, request);
    }

    // 3) Try to delete
    if (unlink(filepath.c_str()) != 0) {
        switch (errno) {
        case EACCES:
        case EPERM:
            return ResponseBuilder::generateError(403, server, request);
        case ENOENT:
            return ResponseBuilder::generateError(404, server, request);
        default:
            return ResponseBuilder::generateError(500, server, request);
        }
    }

    // 4) Success page
    std::string filename = request.getPath().substr(request.getPath().find_last_of('/') + 1);
    std::string body     = "<h1>File " + filename + " deleted.</h1>";
    return ResponseBuilder::generateSuccess(200, body, "text/html", request);
}