/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleGet.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/15 12:39:41 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/05 17:23:53 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <algorithm>
#include <chrono>
#include <ctime>
#include <dirent.h>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <vector>

#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/methodsHandler.hpp"
#include "http/responseBuilder.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/stringUtils.hpp"

HttpResponse handleGet(const HttpRequest& request, const Server& server, const Location& loc) {

    std::string filepath = resolvePhysicalPath(request, loc);

    if (isSymlink(filepath)) {
        return ResponseBuilder::generateError(403, server, request);
    }

    struct stat fileStat;
    if (stat(filepath.c_str(), &fileStat) == 0) {
        const std::string& uri = request.getPath();

        if (S_ISDIR(fileStat.st_mode)) {

            std::string normalized = normalizePath(uri);
            if (normalized.empty()) {
                return ResponseBuilder::generateError(403, server, request);
            }
            if (!normalized.empty() && normalized.back() != '/') {
                return ResponseBuilder::generateRedirect(301, normalized + "/", request);
            }

            std::string index_file = loc.getIndex();
            if (!index_file.empty()) {
                std::string index_path = joinPath(filepath, index_file);

                if (isFile(index_path)) {
                    return serveFile(index_path, request, "");
                }

                if (loc.isAutoindexEnabled()) {
                    return generateAutoindex(filepath, uri, request, server);
                }

                return ResponseBuilder::generateError(403, server, request);
            }

            if (loc.isAutoindexEnabled()) {
                return generateAutoindex(filepath, uri, request, server);
            }
            return ResponseBuilder::generateError(403, server, request);
        }

        if (S_ISREG(fileStat.st_mode)) {
            return serveFile(filepath, request, "");
        }
    }

    return ResponseBuilder::generateError(404, server, request);
}
