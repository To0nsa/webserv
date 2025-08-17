/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   generateAutoindex.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/06 09:08:33 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/17 11:56:38 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <chrono>
#include <filesystem>
#include <format>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/responseBuilder.hpp"
#include "utils/Logger.hpp"
#include "utils/htmlUtils.hpp"
#include "utils/stringUtils.hpp"

namespace fs = std::filesystem;

namespace {

// 1) Scan directory entries: separate names into dirs/files, keep “..” first if present
struct DirEntry {
    std::string    name;
    std::uintmax_t size;
    std::time_t    mtime;
    bool           isDir;
};

namespace fs = std::filesystem;

std::vector<DirEntry> listDirectoryEntries(const std::string& path, bool& ok) {
    std::vector<DirEntry> entries;
    ok = true;

    fs::path dirPath(path);
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        ok = false;
        return {};
    }

    // Try to construct a directory_iterator without throwing:
    std::error_code        dirEc;
    fs::directory_iterator it(dirPath, dirEc);
    if (dirEc) {
        ok = false;
        return {};
    }

    // Now loop in a non‐throwing way:
    std::error_code entryEc;
    for (; it != fs::directory_iterator{}; it.increment(entryEc)) {
        if (entryEc) {
            entryEc.clear();
            continue;
        }

        fs::path        p = it->path();
        std::error_code statEc;

        fs::file_time_type ftime = fs::last_write_time(p, statEc);
        if (statEc)
            continue;

        DirEntry entry;
        entry.name = p.filename().string();

        std::error_code dirCheckEc;
        entry.isDir = fs::is_directory(p, dirCheckEc) && !dirCheckEc;

        if (entry.isDir) {
            entry.size = 0ULL;
        } else {
            entry.size = fs::file_size(p, statEc);
            if (statEc)
                entry.size = 0ULL;
        }

        // Convert ftime → time_t
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        entry.mtime = std::chrono::system_clock::to_time_t(sctp);

        entries.push_back(entry);
    }

    // Insert “..” if parent exists:
    fs::path parent = dirPath.parent_path();
    if (!parent.empty() && fs::exists(parent)) {
        DirEntry up;
        up.name  = "..";
        up.isDir = true;
        up.size  = 0;
        up.mtime = 0;
        entries.insert(entries.begin(), up);
    }

    // Sort “..” at front, then other dirs, then files:
    std::vector<DirEntry> dirs, files;
    for (auto const& e : entries) {
        if (e.name == "..")
            continue;
        if (e.isDir)
            dirs.push_back(e);
        else
            files.push_back(e);
    }
    auto cmp = [](auto const& a, auto const& b) { return a.name < b.name; };
    std::sort(dirs.begin(), dirs.end(), cmp);
    std::sort(files.begin(), files.end(), cmp);

    std::vector<DirEntry> sorted;
    if (!entries.empty() && entries[0].name == "..")
        sorted.push_back(entries[0]);
    sorted.insert(sorted.end(), dirs.begin(), dirs.end());
    sorted.insert(sorted.end(), files.begin(), files.end());
    return sorted;
}

std::string escapeUriComponent(const std::string& s) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            oss << c;
        } else {
            oss << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }
    return oss.str();
}

std::string formatMTimeUTC(std::time_t t) {
    // Convert time_t → chrono::sys_time
    std::chrono::sys_time<std::chrono::seconds> tp{std::chrono::seconds{t}};

    // Round to minutes to match "%d-%b-%Y %H:%M"
    auto tp_minutes = std::chrono::floor<std::chrono::minutes>(tp);

    return std::format("{:%d-%b-%Y %H:%M}", tp_minutes);
}

void renderRow(std::ostringstream& body, const DirEntry& entry, const std::string& baseUri) {
    bool isParent = (entry.name == "..");
    bool isDir    = entry.isDir;

    std::string href;
    std::string disp = entry.name;
    if (isParent) {
        href = baseUri + "../";
        disp = "../";
    } else {
        href = baseUri + escapeUriComponent(entry.name);
        if (isDir) {
            disp += "/";
            href += "/";
        }
    }

    std::string icon  = isDir ? "📁" : "📄";
    std::string mtime = isParent ? "-" : formatMTimeUTC(entry.mtime);
    std::string size  = (isDir || isParent) ? "-" : std::to_string(entry.size);

    body << "    <tr>" << "<td><a href=\"" << htmlEscape(href) << "\">" << icon << " "
         << htmlEscape(disp) << "</a></td>" << "<td>" << mtime << "</td>" << "<td>" << size
         << "</td>" << "</tr>\n";
}

std::string htmlHeader(const std::string& uri) {
    std::ostringstream ss;
    ss << R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Index of )"
       << htmlEscape(uri) << R"(</title>
</head>
<body>
  <h1>Index of )"
       << htmlEscape(uri) << R"(</h1>
  <table>
    <tr><th>Name</th><th>Last Modified (UTC)</th><th>Size</th></tr>
)";
    return ss.str();
}

std::string htmlFooter() {
    return std::string("  </table>\n</body>\n</html>");
}

} // anonymous namespace

HttpResponse generateAutoindex(const std::string& filepath, const std::string& uri,
                               const HttpRequest& request, const Server& server) {
    bool dirOk;
    auto entries = listDirectoryEntries(filepath, dirOk);
    if (!dirOk) {
        Logger::logFrom(LogLevel::WARN, "Autoindex",
                        "Cannot open directory: " + filepath +
                            " → rejecting autoindex for URI: " + request.getPath());
        return ResponseBuilder::generateError(403, server, request);
    }

    // Ensure baseUri ends with “/”
    std::string baseUri = uri;
    if (baseUri.empty() || baseUri.back() != '/')
        baseUri += '/';

    // Build the HTML body
    std::ostringstream body;
    body << htmlHeader(uri);
    for (auto const& e : entries) {
        renderRow(body, e, baseUri);
    }
    body << htmlFooter();

    Logger::logFrom(LogLevel::INFO, "Autoindex",
                    "Generated autoindex for URI: " + request.getPath() +
                        " (directory: " + filepath + ")");
    return ResponseBuilder::generateSuccess(200, body.str(), "text/html", request);
}
