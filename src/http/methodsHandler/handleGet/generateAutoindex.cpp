/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   generateAutoindex.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/06 09:08:33 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/19 10:53:24 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    generateAutoindex.cpp
 * @brief   Implements directory autoindex (HTML listing) generation.
 *
 * @details This file provides nginx-like autoindex functionality for HTTP GET
 *          requests on directories when no index file is found and autoindexing
 *          is enabled in the location block.
 *
 *          Main responsibilities:
 *          - **Directory scanning** (@ref listDirectoryEntries):
 *              Collects entries, separates directories from files, preserves `..`.
 *          - **Escaping utilities**:
 *              - @ref escapeUriComponent for safe URIs.
 *              - @ref htmlEscape (from utils/htmlUtils) for HTML-safe output.
 *          - **Formatting helpers**:
 *              - @ref formatMTimeUTC for human-readable UTC timestamps.
 *              - @ref renderRow for HTML table rows with icons and metadata.
 *          - **HTML layout**:
 *              - @ref htmlHeader and @ref htmlFooter wrap the index table.
 *          - **Entrypoint**: @ref generateAutoindex assembles the full HTML
 *            directory listing and returns a @ref HttpResponse.
 *
 *          Errors (nonexistent or inaccessible directories) result in 403
 *          responses. Valid directories produce a simple HTML table with:
 *          - File/directory name (with 📄/📁 icons).
 *          - Last modified timestamp (UTC).
 *          - File size (bytes).
 *
 * @ingroup request_handler
 */

#include "http/HttpRequest.hpp"     // for HttpRequest
#include "http/HttpResponse.hpp"    // for HttpResponse
#include "http/responseBuilder.hpp" // for generateError, generateSuccess
#include "utils/Logger.hpp"         // for LogLevel, Logger
#include "utils/htmlUtils.hpp"      // for htmlEscape
#include <algorithm>                // for max, sort
#include <bits/chrono.h>            // for duration, operator-, floor, oper...
#include <cctype>                   // for isalnum
#include <chrono>                   // for operator/
#include <compare>                  // for operator<
#include <cstdint>                  // for uintmax_t
#include <ctime>                    // for time_t
#include <filesystem>               // for path, directory_iterator, exists
#include <iomanip>                  // for operator<<, setfill, setw
#include <sstream>                  // for basic_ostream, operator<<, basic...
#include <string>                   // for char_traits, allocator, operator+
#include <system_error>             // for error_code
#include <vector>                   // for vector
class Server;

namespace fs = std::filesystem;

namespace {

/**
 * @struct DirEntry
 * @brief  Represents a single entry in a directory for autoindexing.
 *
 * @details Used by @ref listDirectoryEntries to store metadata for each
 *          file or subdirectory. Provides the minimal information needed
 *          to render an autoindex table row in HTML.
 *
 * @var DirEntry::name
 *      The entry’s filename (e.g., `"file.txt"` or `"subdir"`).
 *
 * @var DirEntry::size
 *      File size in bytes. For directories and `".."`, this is set to `0`.
 *
 * @var DirEntry::mtime
 *      Last modification time (UTC, as `time_t`). Used for “Last Modified” column.
 *
 * @var DirEntry::isDir
 *      Whether the entry is a directory (`true`) or a regular file (`false`).
 *
 * @ingroup request_handler
 */
struct DirEntry {
    std::string    name;
    std::uintmax_t size;
    std::time_t    mtime;
    bool           isDir;
};

namespace fs = std::filesystem;

/**
 * @brief Lists and sorts directory entries for autoindex generation.
 *
 * @details Scans the given filesystem path and collects metadata into
 *          @ref DirEntry objects. Handles errors in a non-throwing way
 *          using `std::error_code`. Results are separated into:
 *          - `".."` (if parent exists, inserted at the front).
 *          - Subdirectories (sorted alphabetically).
 *          - Files (sorted alphabetically).
 *
 *          Each entry records:
 *          - `name`: filename or `".."`.
 *          - `isDir`: directory vs file.
 *          - `size`: file size in bytes (0 for directories).
 *          - `mtime`: last modification time (UTC).
 *
 * @param path Filesystem path to the directory.
 * @param ok   Reference flag set to `false` if the directory
 *             cannot be opened or scanned safely.
 *
 * @return A vector of @ref DirEntry objects sorted for autoindex rendering.
 *
 * @ingroup request_handler
 */
std::vector<DirEntry> listDirectoryEntries(const std::string& path, bool& ok) {
    std::vector<DirEntry> entries;
    ok = true;

    fs::path dirPath(path);
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        ok = false;
        return {};
    }

    // Try non-throwing directory iterator
    std::error_code        dirEc;
    fs::directory_iterator it(dirPath, dirEc);
    if (dirEc) {
        ok = false;
        return {};
    }

    // Walk entries safely (clear errors on failure)
    std::error_code entryEc;
    for (; it != fs::directory_iterator{}; it.increment(entryEc)) {
        if (entryEc) { // skip unreadable entries
            entryEc.clear();
            continue;
        }

        fs::path        p = it->path();
        std::error_code statEc;

        // Get last modification time
        fs::file_time_type ftime = fs::last_write_time(p, statEc);
        if (statEc)
            continue;

        DirEntry entry;
        entry.name = p.filename().string();

        // Detect directory
        std::error_code dirCheckEc;
        entry.isDir = fs::is_directory(p, dirCheckEc) && !dirCheckEc;

        // Get size (only for files)
        if (entry.isDir) {
            entry.size = 0ULL;
        } else {
            entry.size = fs::file_size(p, statEc);
            if (statEc)
                entry.size = 0ULL;
        }

        // Convert ftime → time_t for portable rendering
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        entry.mtime = std::chrono::system_clock::to_time_t(sctp);

        entries.push_back(entry);
    }

    // Insert “..” if parent directory exists
    fs::path parent = dirPath.parent_path();
    if (!parent.empty() && fs::exists(parent)) {
        DirEntry up;
        up.name  = "..";
        up.isDir = true;
        up.size  = 0;
        up.mtime = 0;
        entries.insert(entries.begin(), up);
    }

    // Sort: “..” first, then directories, then files (alphabetically)
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

    // Build sorted result
    std::vector<DirEntry> sorted;
    if (!entries.empty() && entries[0].name == "..")
        sorted.push_back(entries[0]); // keep parent first
    sorted.insert(sorted.end(), dirs.begin(), dirs.end());
    sorted.insert(sorted.end(), files.begin(), files.end());
    return sorted;
}

/**
 * @brief Percent-encodes a string for safe inclusion in a URI component.
 *
 * @details Encodes all characters except the unreserved set defined by RFC 3986:
 *          `ALPHA / DIGIT / "-" / "_" / "." / "~"`.
 *          Other bytes are converted into `%HH` format using uppercase hex digits.
 *
 * @param s Input string (raw filename or path segment).
 *
 * @return A URI-safe string with reserved characters percent-encoded.
 *
 * @note This is used when generating autoindex links to ensure that spaces,
 *       control characters, and unsafe symbols don’t break the resulting URL.
 *
 * @ingroup request_handler
 */
std::string escapeUriComponent(const std::string& s) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex;

    for (unsigned char c : s) {
        // Allowed characters remain unchanged
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            oss << c;
        } else {
            // Encode everything else as %HH
            oss << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }
    return oss.str();
}

/**
 * @brief Formats a file modification time into a human-readable UTC string.
 *
 * @details Converts a `time_t` (last modification time) into a textual
 *          representation using `gmtime()` and `std::put_time`.
 *          The format is: `DD-Mon-YYYY HH:MM` (UTC).
 *
 * @param t Last modification time as `std::time_t`.
 *
 * @return A formatted string (e.g., `"19-Aug-2025 09:42"`) or an empty
 *         string if conversion fails.
 *
 * @note This is used in autoindex HTML tables for the **Last Modified** column.
 *
 * @ingroup request_handler
 */
std::string formatMTimeUTC(std::time_t t) {
    std::tm* gmPtr = std::gmtime(&t); // Convert to UTC (thread-unsafe)
    if (!gmPtr) {
        return {};
    }
    std::ostringstream ss;
    ss << std::put_time(gmPtr, "%d-%b-%Y %H:%M"); // Format: "19-Aug-2025 09:42"
    return ss.str();
}

/**
 * @brief Renders a single `<tr>` row in the autoindex HTML table.
 *
 * @details Converts a directory entry into a clickable HTML link with
 *          metadata (last modified, size).
 *          - The `".."` entry is treated as a parent directory link.
 *          - Directories get a trailing slash (`/`) in both display and href.
 *          - Files show their size in bytes, while directories/`".."` show `"-"`.
 *          - Icons: 📁 for directories, 📄 for files.
 *
 * @param body    Output stringstream accumulating the HTML response.
 * @param entry   Directory entry metadata (name, size, mtime, type).
 * @param baseUri Base URI (must end with `/`) to prepend to href links.
 *
 * @note This function applies both @ref escapeUriComponent (for href safety)
 *       and @ref htmlEscape (for display safety), preventing XSS or broken links.
 *
 * @ingroup request_handler
 */
void renderRow(std::ostringstream& body, const DirEntry& entry, const std::string& baseUri) {
    bool isParent = (entry.name == "..");
    bool isDir    = entry.isDir;

    // Construct href and display name
    std::string href;
    std::string disp = entry.name;
    if (isParent) {
        href = baseUri + "../"; // parent directory link
        disp = "../";
    } else {
        href = baseUri + escapeUriComponent(entry.name); // percent-encode special chars
        if (isDir) {
            disp += "/";
            href += "/";
        }
    }

    // Metadata columns
    std::string icon  = isDir ? "📁" : "📄";
    std::string mtime = isParent ? "-" : formatMTimeUTC(entry.mtime);
    std::string size  = (isDir || isParent) ? "-" : std::to_string(entry.size);

    // Append HTML table row
    body << "    <tr>" << "<td><a href=\"" << htmlEscape(href) << "\">" << icon << " "
         << htmlEscape(disp) << "</a></td>" << "<td>" << mtime << "</td>" << "<td>" << size
         << "</td>" << "</tr>\n";
}

/**
 * @brief Generates the opening HTML markup for an autoindex page.
 *
 * @details Builds the document `<head>`, page title, and initial table
 *          structure for directory listing.
 *          The provided @p uri is escaped with @ref htmlEscape to prevent
 *          XSS and is displayed in both the `<title>` and `<h1>` headers.
 *
 * @param uri The request URI being indexed (e.g. "/images/").
 * @return HTML string containing the document header and table headers.
 *
 * @ingroup request_handler
 */
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

/**
 * @brief Generates the closing HTML markup for an autoindex page.
 *
 * @details Completes the open `<table>`, closes `<body>`, and terminates
 *          the HTML document. This function should always be called after
 *          @ref htmlHeader and rows rendered with @ref renderRow.
 *
 * @return HTML string containing the footer markup.
 *
 * @ingroup request_handler
 */
std::string htmlFooter() {
    return std::string("  </table>\n</body>\n</html>");
}
} // anonymous namespace

/**
 * @brief Generates an HTML autoindex page for a directory listing.
 *
 * @details This function inspects the given filesystem path, enumerates
 *          its directory entries via @ref listDirectoryEntries, and
 *          produces an HTML table showing subdirectories and files with
 *          name, last modification time, and size.
 *
 *          Workflow:
 *          1. **Directory validation**: Ensure the target path exists and is
 *             accessible. If not, return a `403 Forbidden` error.
 *          2. **Base URI normalization**: Guarantee that the URI ends with
 *             a trailing slash (`/`) for proper link resolution.
 *          3. **HTML generation**: Build the autoindex page by composing
 *             @ref htmlHeader, calling @ref renderRow for each entry, and
 *             closing with @ref htmlFooter.
 *          4. **Response assembly**: Return a `200 OK` response with the
 *             generated HTML body and `text/html` MIME type.
 *
 * @param filepath Filesystem path to the directory being indexed.
 * @param uri      Request URI corresponding to the directory.
 * @param request  Incoming HTTP request context.
 * @param server   Server configuration (used for error responses).
 *
 * @return HttpResponse with a full HTML autoindex page on success,
 *         or an error response (403) if the directory cannot be accessed.
 *
 * @ingroup request_handler
 */
HttpResponse generateAutoindex(const std::string& filepath, const std::string& uri,
                               const HttpRequest& request, const Server& server) {
    bool dirOk;
    auto entries = listDirectoryEntries(filepath, dirOk);
    if (!dirOk) {
        // Directory not accessible → return 403 Forbidden
        Logger::logFrom(LogLevel::WARN, "Autoindex",
                        "Cannot open directory: " + filepath +
                            " → rejecting autoindex for URI: " + request.getPath());
        return ResponseBuilder::generateError(403, server, request);
    }

    // Ensure base URI ends with “/” for proper relative link building
    std::string baseUri = uri;
    if (baseUri.empty() || baseUri.back() != '/')
        baseUri += '/';

    // Build the HTML body: header + rows + footer
    std::ostringstream body;
    body << htmlHeader(uri);
    for (auto const& e : entries) {
        renderRow(body, e, baseUri); // Add a row for each directory/file entry
    }
    body << htmlFooter();

    // Log and return success response with generated HTML
    Logger::logFrom(LogLevel::INFO, "Autoindex",
                    "Generated autoindex for URI: " + request.getPath() +
                        " (directory: " + filepath + ")");
    return ResponseBuilder::generateSuccess(200, body.str(), "text/html", request);
}
