/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_get.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/15 12:39:41 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/04 22:37:35 by nlouis           ###   ########.fr       */
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
#include "http/HttpResponseBuilder.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/stringUtils.hpp"

static HttpResponse generateAutoindex(const std::string& filepath, const std::string& uri,
                                      const HttpRequest& request, const Server& server) {
    DIR* dir = opendir(filepath.c_str());
    if (!dir)
        return ResponseBuilder::generateError(403, server, request);

    std::vector<std::string> dirs, files;
    struct dirent*           entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == ".")
            continue;
        if (name == "..") {
            // keep “..” at the front, but still render it like any other directory
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

    // Sort but leave “..” at index 0 if present
    if (!dirs.empty() && dirs[0] == "..")
        std::sort(dirs.begin() + 1, dirs.end());
    else
        std::sort(dirs.begin(), dirs.end());

    std::sort(files.begin(), files.end());

    std::stringstream body;
    std::string       baseUri = uri;
    if (baseUri.empty() || baseUri.back() != '/')
        baseUri += '/';

    body << R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Index of )"
         << uri << R"(</title>
  <style>
    body {
      background: #1e1e1e;
      color: #dcdcdc;
      font-family: "Segoe UI", sans-serif;
      padding: 2rem;
    }
    h1 {
      color: #4fc3f7;
      margin-bottom: 1rem;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      margin-top: 1rem;
    }
    th, td {
      padding: 0.75rem 1.5rem;
      text-align: left;
      font-family: monospace;
      border-bottom: 1px solid #333;
    }
    tr:hover {
      background-color: #2e2e2e;
    }
    a {
      color: #81d4fa;
      text-decoration: none;
    }
    a:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
  <h1>Index of )"
         << uri << R"(</h1>
  <table>
    <tr><th>Name</th><th>Last Modified (UTC)</th><th>Size</th></tr>
)";

    auto escapeUriComponent = [](const std::string& name) -> std::string {
        std::ostringstream oss;
        for (unsigned char c : name) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
                oss << c;
            else
                oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(c);
        }
        return oss.str();
    };

    auto renderEntry = [&](const std::string& name) {
        // Compute the actual path on disk (for “..”, this points to parent)
        std::string fullPath;
        bool        isParent = (name == "..");
        if (isParent)
            fullPath = filepath + "/..";
        else
            fullPath = filepath + "/" + name;

        struct stat fileStat;
        if (stat(fullPath.c_str(), &fileStat) == -1)
            return;

        bool isDir = S_ISDIR(fileStat.st_mode);

        // Format the modification time (UTC) of this entry:
        std::time_t mod_secs = fileStat.st_mtime;
        std::tm*    gm       = std::gmtime(&mod_secs);
        if (gm == nullptr)
            return; // if gmtime fails, skip

        std::ostringstream timeStream;
        timeStream << std::put_time(gm, "%d-%b-%Y %H:%M");

        // Build href/display name:
        std::string href;
        std::string displayName = name;
        if (isParent) {
            href        = baseUri + "../"; // “..” should link one level up
            displayName = "../";
        } else {
            href = baseUri + escapeUriComponent(name);
            if (isDir) {
                displayName += "/";
                href += "/";
            }
        }

        // Choose an icon
        std::string icon = isDir ? "📁" : "📄";

        // Determine size: for directories (including “..”), show "-" ; for files, show actual size
        std::string sizeStr =
            isDir ? std::string("-") : std::to_string(static_cast<long long>(fileStat.st_size));

        body << "<tr>";
        body << "<td><a href=\"" << href << "\">" << icon << " " << displayName << "</a></td>";
        body << "<td>" << timeStream.str() << "</td>";
        body << "<td>" << sizeStr << "</td>";
        body << "</tr>\n";
    };

    // Render all directories (including “..” if present), then files:
    for (const std::string& d : dirs)
        renderEntry(d);
    for (const std::string& f : files)
        renderEntry(f);

    body << R"(</table>
</body>
</html>)";

    return ResponseBuilder::generateSuccess(200, body.str(), "text/html", request);
}

/* static std::string truncateName(const std::string& name, std::size_t maxLen) {
    if (name.length() <= maxLen)
        return name;
    if (maxLen <= 2)
        return std::string(maxLen, '.'); // fallback: "..", ".", or ""
    return name.substr(0, maxLen - 2) + "..";
} */

/* static HttpResponse generateAutoindex(const std::string& filepath, const std::string& uri,
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
} */

HttpResponse handleGet(const HttpRequest& request, const Server& server, const Location& loc) {
    std::string requestPath   = normalizePath(request.getPath());
    std::string locPrefix     = normalizePath(loc.getPath());
    bool        inUploadStore = loc.isUploadEnabled() && requestPath.rfind(locPrefix, 0) == 0;

    std::string filepath;
    if (inUploadStore) {
        std::string relative = requestPath.substr(locPrefix.size());
        while (!relative.empty() && relative.front() == '/')
            relative.erase(0, 1);

        std::string uploadRoot = normalizePath(loc.getUploadStore());
        if (!uploadRoot.empty() && uploadRoot.front() != '/')
            uploadRoot = joinPath(normalizePath(loc.getRoot()), uploadRoot);

        filepath = joinPath(uploadRoot, relative);
        std::cout << "[GET] In upload_store. Upload root: " << uploadRoot
                  << ", relative: " << relative << std::endl;
    } else {
        filepath = buildFilePath(request, loc);
        std::cout << "[GET] In regular static file mode. buildFilePath() result: " << filepath
                  << std::endl;
    }

    std::cout << "[GET] Final resolved file path: " << filepath << std::endl;

    if (isSymlink(filepath)) {
        std::cerr << "[GET] ❌ Refusing to serve symlink: " << filepath << std::endl;
        return ResponseBuilder::generateError(403, server, request);
    }

    struct stat fileStat;
    if (stat(filepath.c_str(), &fileStat) == 0) {
        const std::string& uri = request.getPath();

        if (S_ISDIR(fileStat.st_mode)) {
            std::cout << "[GET] 📁 Path is a directory.\n";

            std::string normalized = normalizePath(uri);
            if (normalized.empty()) {
                return ResponseBuilder::generateError(403, server, request);
            }
            if (!normalized.empty() && normalized.back() != '/') {
                std::cout << "[GET] ↪️ Redirecting to URI with trailing slash: " << normalized + "/"
                          << std::endl;
                return ResponseBuilder::generateRedirect(301, normalized + "/", request);
            }

            std::string index_file = loc.getIndex();
            if (!index_file.empty()) {
                std::string index_path = joinPath(filepath, index_file);
                std::cout << "[GET] 🔍 Looking for index file: " << index_path << std::endl;

                if (isFile(index_path)) {
                    std::cout << "[GET] ✅ Found index file, serving it.\n";
                    return serveFile(index_path, request, "");
                }

                if (loc.isAutoindexEnabled()) {
                    std::cout
                        << "[GET] 📄 Index missing, but autoindex is ON — generating listing.\n";
                    return generateAutoindex(filepath, uri, request, server);
                }

                std::cerr << "[GET] ❌ Index file specified but not found: " << index_path
                          << std::endl;
                return ResponseBuilder::generateError(403, server, request);
            }

            if (loc.isAutoindexEnabled()) {
                std::cout << "[GET] 📄 No index file. Autoindex is ON — generating listing.\n";
                return generateAutoindex(filepath, uri, request, server);
            }

            std::cerr << "[GET] ❌ No index and autoindex is OFF — forbidden.\n";
            return ResponseBuilder::generateError(403, server, request);
        }

        if (S_ISREG(fileStat.st_mode)) {
            std::cout << "[GET] 📄 Path is a regular file — serving it.\n";
            return serveFile(filepath, request, "");
        }

        std::cerr << "[GET] ❌ Path exists but is not a file or dir: " << filepath << std::endl;
    } else {
        std::cerr << "[GET] ❌ stat() failed — file not found: " << filepath << std::endl;
    }

    return ResponseBuilder::generateError(404, server, request);
}
