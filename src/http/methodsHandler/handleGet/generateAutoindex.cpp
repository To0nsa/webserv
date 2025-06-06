/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   generateAutoindex.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/05 17:21:15 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/05 17:21:53 by nlouis           ###   ########.fr       */
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
#include "http/responseBuilder.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/stringUtils.hpp"

HttpResponse generateAutoindex(const std::string& filepath, const std::string& uri,
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