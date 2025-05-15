/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_get.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/15 12:39:41 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/15 20:12:56 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handle_get.hpp"
#include "utils/buildFilePath.hpp"
#include <string_view>

static std::string truncateName(const std::string& name, std::size_t maxLen) {
    if (name.length() <= maxLen)
        return name;
    if (maxLen <= 2)
        return std::string(maxLen, '.'); // fallback: "..", ".", or ""
    return name.substr(0, maxLen - 2) + "..";
}

HttpResponse generateAutoindex(const std::string& filepath, const std::string& uri,
                               const HttpRequest& request, const Server& server) {
    DIR* dir = opendir(filepath.c_str());
    if (!dir)
        return ResponseBuilder::generateError(403, server, request);

    std::vector<std::string> dirs, files;
    struct dirent*           entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == ".")
            continue;
        if (name == "..") {
            dirs.insert(dirs.begin(), name);
            continue;
        }
        std::string fullPath = filepath + "/" + name;
        struct stat fileStat;
        if (stat(fullPath.c_str(), &fileStat) == -1)
            continue;
        if (S_ISDIR(fileStat.st_mode))
            dirs.push_back(name);
        else
            files.push_back(name);
    }
    closedir(dir);

    if (!dirs.empty() && dirs[0] == "..")
        std::sort(dirs.begin() + 1, dirs.end());
    else
        std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());

    std::stringstream body;
    std::string       baseUri = uri;
    if (baseUri.empty() || baseUri.back() != '/')
        baseUri += '/';

    body << "<html><body><h1>&nbsp;&nbsp; Index of " << uri << "</h1>";
    body << "<table><tr><th>Name</th><th>Last Modified (UTC)</th><th>Size (bytes)</th></tr>";

    auto renderEntry = [&](const std::string& name) {
        std::string fullPath = filepath + "/" + name;
        struct stat fileStat;
        if (stat(fullPath.c_str(), &fileStat) == -1)
            return;

        std::stringstream timeStream;
        auto              mod_time = std::chrono::system_clock::from_time_t(fileStat.st_mtime);
        std::time_t       time     = std::chrono::system_clock::to_time_t(mod_time);
        timeStream << std::put_time(std::gmtime(&time), "%d-%b-%Y %H:%M");

        std::string displayName = truncateName(name, 25);
        std::string href        = baseUri + name;
        bool        isDir       = S_ISDIR(fileStat.st_mode);
        if (isDir) {
            displayName += "/";
            href += "/";
        }

        body << "<tr>";
        body << "<td style='min-width: 20ch; padding-right: 30px;'><a href='" << href << "'>"
             << displayName << "</a></td>";
        body << "<td style='min-width: 20ch; padding-right: 30px;'>" << timeStream.str() << "</td>";
        body << "<td style='min-width: 20ch; padding-right: 30px;'>";
        body << (isDir ? "-" : std::to_string(fileStat.st_size));
        body << "</td></tr>";
    };

    if (!dirs.empty() && dirs[0] == "..") {
        body << "<tr><td colspan='3'><a href='" << baseUri << "../'>../</a></td></tr>";
        dirs.erase(dirs.begin());
    }
    for (const std::string& dirName : dirs)
        renderEntry(dirName);
    for (const std::string& fileName : files)
        renderEntry(fileName);

    body << "</table></body></html>";
    return ResponseBuilder::generateSuccess(200, body.str(), "text/html", request);
}

HttpResponse handleGet(const HttpRequest& request, const Server& server, const Location& loc) {
    std::string filepath = buildFilePath(request, loc);
    std::cout << "Resolved file path: " << filepath << std::endl;

    // Redirect if directory is missing trailing slash
    struct stat fileStat;
    if (stat(filepath.c_str(), &fileStat) == 0) {
        const std::string& uri = request.getPath();
        // Redirect to add trailing slash if it's a directory but URI lacks slash
        if (S_ISDIR(fileStat.st_mode)) {
            if (!uri.empty() && uri.back() != '/') {
                std::string redirectUri = uri + "/";
                return ResponseBuilder::generateRedirect(301, redirectUri, request);
            }
        }
        if (S_ISREG(fileStat.st_mode)) {
            // It's a regular file
            return serveFile(filepath, request, "");
        } else if (S_ISDIR(fileStat.st_mode)) {
            // It's a directory
            std::string index_file = loc.getIndex();
            if (!index_file.empty()) {
                // Serve index.html if it exists in the directory
                std::string index_path = joinPath(filepath, index_file);
                if (fileExists(index_path)) {
                    return serveFile(index_path, request, "");
                }
            }
            // If no index file is found, check if autoindex is enabled
            if (loc.isAutoindexEnabled()) {
                return generateAutoindex(filepath, uri, request, server);
            }
        }
    }
    return ResponseBuilder::generateError(404, server, request);
}
