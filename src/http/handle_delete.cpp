/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_delete.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/21 15:06:07 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/31 13:20:43 by nlouis           ###   ########.fr       */
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
    // 1) Normalize both the request path and the location prefix
    std::string requestPath   = normalizePath(request.getPath());
    std::string locPrefix     = normalizePath(loc.getPath());
    bool        inUploadStore = loc.isUploadEnabled() && requestPath.rfind(locPrefix, 0) == 0;

    // 2) Compute the real filesystem path
    std::string filepath;
    if (inUploadStore) {
        // — Strip the location prefix from the request URI
        std::string relative = requestPath.substr(locPrefix.size());
        while (!relative.empty() && relative.front() == '/')
            relative.erase(0, 1);

        // — Find upload_store base, interpreting relative paths under loc.getRoot()
        std::string uploadRoot = normalizePath(loc.getUploadStore());
        if (!uploadRoot.empty() && uploadRoot.front() != '/')
            uploadRoot = joinPath(normalizePath(loc.getRoot()), uploadRoot);

        // — Final path under upload_store
        filepath = joinPath(uploadRoot, relative);
    } else {
        // Static content (DELETE outside upload_store)
        filepath = buildFilePath(request, loc);
    }

    std::cout << "Resolved file path: " << filepath << std::endl;

    // 3) ANY symlink → Immediately reject with 403
    //    (no more “follow‐and‐delete” logic)
    if (isSymlink(filepath)) {
        return ResponseBuilder::generateError(403, server, request);
    }

    // 4) Must exist
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        std::cerr << "[DELETE] File not found: " << filepath << std::endl;
        return ResponseBuilder::generateError(404, server, request);
    }

    // 5) Must be a regular file (directories, sockets, etc. → 403)
    if (!S_ISREG(st.st_mode)) {
        return ResponseBuilder::generateError(403, server, request);
    }

    // 6) Trailing‐slash edge case: “/foo.txt/” → 404
    if (request.getPath().back() == '/' && S_ISREG(st.st_mode)) {
        return ResponseBuilder::generateError(404, server, request);
    }

    // 7) Try to unlink
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

    // 8) Success: return a simple “deleted” HTML page
    std::string filename = request.getPath().substr(request.getPath().find_last_of('/') + 1);
    std::string body     = "<h1>File " + filename + " deleted.</h1>";
    return ResponseBuilder::generateSuccess(200, body, "text/html", request);
}