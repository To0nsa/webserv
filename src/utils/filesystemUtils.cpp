/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   filesystemUtils.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/13 09:39:07 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 15:58:05 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    filesystemUtils.cpp
 * @brief   Implements filesystem utility functions for Webserv.
 *
 * @details Provides helper routines for safely manipulating paths and
 *          filesystem resources in the context of an HTTP server. This includes:
 *            - Canonicalizing and joining URI and filesystem paths.
 *            - Mapping request URIs to location roots and upload stores.
 *            - Generating safe filenames for uploads.
 *            - Creating directories recursively.
 *            - Verifying file and symlink existence.
 *          The utilities here are designed to prevent directory traversal,
 *          enforce upload root boundaries, and integrate cleanly with
 *          Webserv's request–response pipeline.
 *
 * @ingroup filesystem_utils
 */

#include "utils/filesystemUtils.hpp"
#include "http/responseBuilder.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <system_error>

namespace fs = std::filesystem;

/**
 * @brief Resolves a request URI to a concrete filesystem path.
 *
 * @details Computes the physical path for a request by:
 *  1) Normalizing the request path and attempting resolution under the location's root.
 *  2) If the resource is not a POST target and exists, returns that path.
 *  3) Otherwise, when uploads are enabled and the URI is within the location prefix,
 *     rewrites the path into the configured upload store (joining with the remaining
 *     relative suffix).
 *  4) Falls back to the root-based path even if it does not exist, allowing later
 *     handlers to emit the appropriate HTTP status (e.g., 404).
 *
 * @ingroup filesystem_utils
 *
 * @param req Incoming HTTP request (method and normalized URI are consulted).
 * @param loc Matched location block providing root, path prefix, and upload settings.
 * @return Fully-resolved absolute filesystem path. Returns an empty string when the
 *         normalized request path is empty (invalid or unsupported path).
 *
 * @throws std::runtime_error Only if helper utilities (e.g., normalization/join) are
 *         documented to throw on invalid inputs (none are thrown in the current logic).
 *
 * @note For POST requests, existing files under the root are intentionally ignored to
 *       avoid accidental overwrite semantics; uploads are routed to the upload store
 *       when configured.
 * @warning Callers should not assume the returned path exists unless they checked it.
 *          Existence checking is deliberately limited to the non‑POST, root‑based branch.
 */
std::string resolvePhysicalPath(const HttpRequest& req, const Location& loc) {
    // Normalize the request path
    std::string requestPath = normalizePath(req.getPath());
    if (requestPath.empty())
        return "";

    // 1) Try resolving against the real root
    std::string rootPath = buildFilePath(req, loc);
    // If this isn't a POST and the file actually exists under root, use it
    if (req.getMethod() != "POST" && !rootPath.empty() && fs::exists(rootPath)) {
        return rootPath;
    }

    // 2) Otherwise, if upload_store is enabled and the URI matches the location prefix,
    //    map into the upload‐store
    std::string locPrefix = normalizePath(loc.getPath());
    if (loc.isUploadEnabled() && requestPath.rfind(locPrefix, 0) == 0) {
        // strip leading slashes from the remainder
        std::string relative = requestPath.substr(locPrefix.size());
        while (!relative.empty() && relative.front() == '/')
            relative.erase(0, 1);

        // figure out the absolute upload‐store path
        std::string uploadRoot = normalizePath(loc.getUploadStore());
        if (!uploadRoot.empty() && uploadRoot.front() != '/')
            uploadRoot = joinPath(normalizePath(loc.getRoot()), uploadRoot);

        return joinPath(uploadRoot, relative);
    }

    // 3) Fallback to the root path (even if it doesn't exist)
    return rootPath;
}

bool isFile(const std::string& path) {
    return fs::exists(path) && fs::is_regular_file(path);
}

/**
 * @brief Builds a unique temporary filename in the system temp directory.
 *
 * @details Generates a candidate path under `std::filesystem::temp_directory_path()`
 *          using a high‑resolution timestamp and a monotonically increasing counter.
 *          Intended for write‑to‑temp then atomic rename flows (e.g., buffering
 *          uploaded request bodies or CGI output before publishing). This function
 *          returns a name only; callers must create/open the file atomically.
 *
 * @ingroup filesystem_utils
 *
 * @param prefix   Logical prefix embedded in the filename (e.g., "upload", "cgi").
 * @param counter  Monotonic per‑process counter; incremented on each call.
 * @return Absolute path string to a temporary file candidate (not created).
 *
 * @throws std::filesystem::filesystem_error If temp directory discovery fails.
 *
 * @note Callers should open with exclusive creation flags (e.g., O_CREAT|O_EXCL)
 *       and retry on collisions to guarantee uniqueness under high concurrency.
 * @warning Not thread‑safe w.r.t. the shared @p counter. Use an atomic or pass a
 *          thread‑local counter if invoked from multiple threads.
 * @todo Consider adding PID and a random component (or switch to unique_path)
 *       to further reduce collision probability.
 */
std::string make_temp_name(const std::string& prefix, unsigned& counter) {
    // 1) Locate the system temp directory (respects platform defaults / env).
    fs::path tmpdir = fs::temp_directory_path();

    // 2) High-precision timestamp (nanoseconds since epoch).
    auto now = std::chrono::high_resolution_clock::now();
    auto ns  = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    // 3) Build "<prefix>_<nanoseconds>_<counter>.tmp"
    std::ostringstream ss;
    ss << prefix << "_" << ns << "_" << counter++ << ".tmp";

    return (tmpdir / ss.str()).string();
}

/**
 * @brief Canonicalizes a URI-style path and rejects traversal.
 *
 * @details Collapses repeated slashes, removes "." segments, processes ".."
 *          by popping the previous segment, and preserves a trailing slash
 *          (except for root). Always returns an absolute form starting with "/".
 *          If a leading ".." would escape the root, returns an empty string to
 *          signal an invalid/unsafe path (caller should treat as 400/403).
 *
 * @ingroup filesystem_utils
 *
 * @param path Raw request path or filesystem-like path (may be relative or absolute).
 * @return Canonical absolute path (e.g., "/a/b" or "/a/b/"). Returns empty string
 *         if normalization would traverse above root.
 *
 * @note Trailing slash is preserved to help callers distinguish "directory intent"
 *       (e.g., for index resolution or directory listing).
 * @warning This function does not perform percent-decoding. Callers should decode
 *          the URI before normalization and validate disallowed bytes.
 */
std::string normalizePath(const std::string& path) {
    if (path.empty())
        return "/";

    // Collapse repeated slashes: "///a//b" -> "/a/b"
    std::string collapsed;
    bool        prevWasSlash = false;
    for (char c : path) {
        if (c == '/') {
            if (!prevWasSlash) {
                collapsed += '/';
                prevWasSlash = true;
            }
        } else {
            collapsed += c;
            prevWasSlash = false;
        }
    }

    // Split and resolve "." and ".." while tracking if caller intended a trailing '/'
    std::vector<std::string> segments;
    std::string              segment;
    std::istringstream       stream(collapsed);
    const bool               hadTrailingSlash = collapsed.back() == '/';

    while (std::getline(stream, segment, '/')) {
        if (segment.empty() || segment == ".")
            continue;
        if (segment == "..") {
            if (!segments.empty()) {
                segments.pop_back();
            } else {
                // Would escape root: signal invalid path to caller
                return "";
            }
        } else {
            segments.push_back(segment);
        }
    }

    // Reassemble, preserving trailing slash intent (except for "/")
    std::string result = "/";
    for (std::size_t i = 0; i < segments.size(); ++i) {
        result += segments[i];
        if (i + 1 < segments.size())
            result += "/";
    }
    if (hadTrailingSlash && result != "/")
        result += "/";

    return result;
}

/**
 * @brief Concatenates a base path and a suffix with a single '/' separator.
 *
 * @details Ensures that there is exactly one directory separator between the
 *          base and suffix, regardless of whether the base already ends with
 *          a slash. Does not normalize or validate the components; callers
 *          should sanitize inputs beforehand (e.g., via @ref normalizePath).
 *          Intended for filesystem path assembly when resolving HTTP request
 *          targets to physical files or upload locations.
 *
 * @ingroup filesystem_utils
 *
 * @param base   Base path (absolute or relative). May end with '/'.
 * @param suffix Path fragment to append (should not begin with '/'
 *               unless intentional).
 * @return Combined path string.
 *
 * @note If @p base is empty, the function returns @p suffix unchanged.
 * @warning This function does not handle '.' or '..' resolution; use only
 *          with trusted, normalized inputs to avoid directory traversal.
 */
std::string joinPath(const std::string& base, const std::string& suffix) {
    if (base.empty())
        return suffix;
    if (base.back() == '/')
        return base + suffix;
    return base + '/' + suffix;
}

/**
 * @brief Maps a request path to a filesystem path under a location root.
 *
 * @details Normalizes the incoming request path and the location's configured
 *          path prefix, verifies that the request path begins with the location
 *          prefix, and returns the location's root joined with the remaining
 *          suffix. If the request path does not match the location path, returns
 *          an empty string to signal no match.
 *
 * @ingroup filesystem_utils
 *
 * @param request Incoming HTTP request (URI is read from @ref HttpRequest::getPath()).
 * @param loc     Matched location block containing `path` (URI prefix) and `root`.
 * @return Absolute or relative filesystem path under `loc.root`. Returns an empty
 *         string if the request path does not begin with the location's path.
 *
 * @note This function does not check whether the resulting file exists; callers
 *       may perform existence or type checks before use.
 * @warning Callers must ensure that `request.getPath()` and `loc.getPath()` are
 *          decoded and safe. Normalization via @ref normalizePath helps prevent
 *          traversal, but upstream input validation is still required.
 * @see normalizePath, joinPath
 */
std::string buildFilePath(const HttpRequest& request, const Location& loc) {
    // Normalize both the request path and the location path prefix.
    std::string req_path = normalizePath(request.getPath());
    std::string loc_path = normalizePath(loc.getPath());
    std::string loc_root = loc.getRoot();

    // Ensure the request path starts with the location prefix.
    if (req_path.rfind(loc_path, 0) != 0) {
        return "";
    }

    // Extract the suffix (portion after the location prefix).
    std::string suffix = req_path.substr(loc_path.size());
    if (!suffix.empty() && suffix[0] == '/')
        suffix.erase(0, 1);

    // Join the location's filesystem root with the relative suffix.
    return joinPath(loc_root, suffix);
}

/* static std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream        ss(path);
    std::string              part;

    // Extract substrings between '/' and skip empty ones.
    while (std::getline(ss, part, '/')) {
        if (!part.empty())
            parts.push_back(part);
    }
    return parts;
} */

/* bool mkdirRecursive(const std::string& path) {
    std::vector<std::string> parts   = splitPath(path);
    std::string              current = path[0] == '/' ? "/" : "";

    for (size_t i = 0; i < parts.size(); ++i) {
        current = joinPath(current, parts[i]);
        if (isFile(current)) {
            return false;
        }
        if (mkdir(current.c_str(), 0777) == -1) {
            if (errno != EEXIST) {
                return false;
            }
        }
    }
    return true;
} */

/**
 * @brief Ensures that a directory exists, creating missing parents as needed.
 *
 * @details Checks if the given path already exists. If it exists as a regular
 *          file, the function fails. If it exists as a directory (or symlink
 *          to a directory), the function succeeds. Otherwise, it attempts to
 *          create the directory and all missing parent directories using
 *          `std::filesystem::create_directories`.
 *
 * @ingroup filesystem_utils
 *
 * @param path Filesystem path to ensure as a directory (absolute or relative).
 * @return `true` if the directory exists or was created successfully,
 *         `false` if the path is an existing regular file or creation failed.
 *
 * @note Uses `std::filesystem` and its error_code overloads to avoid exceptions.
 * @warning Does not check for permissions beyond creation attempts; callers
 *          should handle permission errors via the return value.
 */
bool mkdirRecursive(const std::string& path) {
    std::error_code ec;

    // If something exists at path and it's a *file*, fail.
    if (fs::exists(path, ec)) {
        if (fs::is_regular_file(path))
            return false;
        return true; // already a directory (or symlink-to-dir)
    }

    // Recursively create any missing directories.
    if (!fs::create_directories(path, ec) && ec) {
        return false; // creation failed for some reason
    }

    return true;
}

bool isSymlink(const std::string& path) {
    return fs::is_symlink(fs::path(path));
}

time_t getCurrentTime() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

/**
 * @brief Generates a timestamp-based fallback filename for uploads.
 *
 * @details Produces a string of the form `"upload_YYYYMMDDHHMMSS"` based on
 *          the current UTC time. Intended for use when the client request
 *          does not provide a valid or safe filename in an upload operation.
 *          This ensures that the uploaded file can still be stored with a
 *          unique, deterministic name that reflects its creation time.
 *
 * @ingroup filesystem_utils
 *
 * @return Fallback filename string in UTC timestamp format.
 *
 * @note Uses `std::gmtime`, which is not thread-safe on some platforms.
 *       If concurrent calls are expected, protect with a mutex or switch
 *       to `std::gmtime_r` (POSIX) or `std::gmtime_s` (Windows).
 * @warning The generated name does not include a file extension; callers
 *          should append one if required.
 */
static std::string makeFallbackName() {
    auto               now = std::chrono::system_clock::now();
    auto               t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << "upload_" << std::put_time(std::gmtime(&t), "%Y%m%d%H%M%S");
    return oss.str();
}

/**
 * @brief Produces a safe filename from untrusted input.
 *
 * @details Strips leading path components, removes any remaining path separators
 *          (`'/'` or `'\\'`) and control characters, while preserving all other
 *          bytes (including valid UTF-8 sequences). If the resulting name is
 *          empty or resolves to `"."` or `".."`, a fallback name from
 *          @ref makeFallbackName is used instead. Intended for securing uploaded
 *          filenames against directory traversal and control character injection.
 *
 * @ingroup filesystem_utils
 *
 * @param raw Untrusted filename (may include paths, separators, or control chars).
 * @return Sanitized filename safe for joining with an upload directory.
 *
 * @note This function does not alter Unicode codepoints except to remove control
 *       characters; callers may wish to normalize Unicode for cross-platform
 *       consistency.
 * @warning The sanitized name is safe for use as a single path component, but
 *          not necessarily unique. Collisions should be handled by the caller.
 */
std::string sanitizeFilename(const std::string& raw) {
    namespace fs = std::filesystem;

    // 1) Drop any leading path components (e.g., "dir/file.txt" -> "file.txt").
    fs::path    p(raw);
    std::string name = p.filename().string();

    // 2) Remove path separators and control characters; keep other bytes (including Unicode).
    name.erase(
        std::remove_if(name.begin(), name.end(),
                       [](unsigned char c) { return c == '/' || c == '\\' || std::iscntrl(c); }),
        name.end());

    // 3) If empty or reserved ".", "..", generate a timestamp-based fallback name.
    if (name.empty() || name == "." || name == "..") {
        name = makeFallbackName();
    }

    return name;
}

/**
 * @brief Resolves a safe absolute path for an uploaded file.
 *
 * @details Given a trusted upload root and an untrusted relative path from a
 *          client request, this function:
 *          1. Canonicalizes the upload root path.
 *          2. Splits the raw relative path on `'/'` and sanitizes each segment
 *             via @ref sanitizeFilename to remove dangerous characters.
 *          3. Canonicalizes the parent directory (so symlinks and `..` are
 *             resolved) while preserving the leaf name even if it does not yet exist.
 *          4. Ensures that the final candidate path is still within the canonical
 *             upload root, rejecting any path that would escape it.
 *
 * @ingroup filesystem_utils
 *
 * @param uploadRoot        Trusted base directory for file uploads.
 * @param rawRelativePath   Untrusted path from client input (relative to root).
 * @return Absolute canonical path string under @p uploadRoot suitable for safe writing.
 *         Returns an empty string on error, invalid input, or if the resolved
 *         path would leave @p uploadRoot.
 *
 * @note Uses `std::filesystem::weakly_canonical` to avoid exceptions and to
 *       handle partially existing paths. If the leaf file does not exist,
 *       only the parent path is canonicalized.
 * @warning This function only ensures the path is inside @p uploadRoot; callers
 *          must still create missing directories and open files securely
 *          (e.g., with `O_CREAT | O_EXCL` to avoid races).
 * @see sanitizeFilename
 */
std::string makeSafeUploadPath(const std::string& uploadRoot, const std::string& rawRelativePath) {
    std::error_code ec;

    // 1) Canonicalize uploadRoot
    fs::path root(uploadRoot);
    fs::path canonRoot = fs::weakly_canonical(root, ec);
    if (ec) {
        // bad uploadRoot
        return {};
    }

    // 2) Split the rawRelativePath on '/' and sanitize each segment
    fs::path          candidate = root;
    std::stringstream ss(rawRelativePath);
    std::string       segment;
    while (std::getline(ss, segment, '/')) {
        if (segment.empty())
            continue;
        std::string safeSeg = sanitizeFilename(segment);
        candidate /= safeSeg;
    }

    // 3) Canonicalize parent while preserving leaf
    fs::path leaf        = candidate.filename();
    fs::path canonParent = fs::weakly_canonical(candidate.parent_path(), ec);
    if (ec) {
        // parent doesn’t exist or broken symlink
        return {};
    }
    fs::path canonCandidate = canonParent / leaf;

    // 4) Bound-check: ensure result is inside canonRoot
    auto rootStr = canonRoot.generic_string();
    auto candStr = canonCandidate.generic_string();
    if (candStr.size() < rootStr.size() || candStr.compare(0, rootStr.size(), rootStr) != 0 ||
        (candStr.size() > rootStr.size() && candStr[rootStr.size()] != '/')) {
        // outside of uploadRoot
        return {};
    }

    return candStr;
}
