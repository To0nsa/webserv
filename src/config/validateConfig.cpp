/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   validateConfig.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/20 23:23:50 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 16:31:16 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    validateConfig.cpp
 * @brief   Implements validation logic for parsed configuration objects.
 *
 * @details This file defines the validation pipeline for the Webserv configuration
 *          after parsing and normalization. It enforces semantic correctness of
 *          `Server` and `Location` blocks, ensuring that directives are consistent,
 *          non-conflicting, and point to valid filesystem resources.
 *
 *          The validation routines cover:
 *          - Presence of at least one location per server
 *          - Correct syntax and uniqueness of location paths
 *          - Required defaults (root or return directive)
 *          - Validity of `server_name` (format, duplicates, uniqueness per host:port)
 *          - Port and host binding conflicts
 *          - Error page code ranges
 *          - Redirect codes restricted to standard 3xx values
 *          - Allowed HTTP methods (`GET`, `POST`, `DELETE`)
 *          - Non-zero client body size limits
 *          - CGI extensions and interpreter mappings
 *          - Existence and type of root and upload directories
 *          - Index file usage with valid roots
 *
 *          Together, these checks guarantee that the configuration is safe to
 *          use at runtime, preventing invalid states and improving user feedback
 *          with actionable error messages.
 *
 * @ingroup config_validation
 */

#include "config/validateConfig.hpp"
#include "config/parser/ConfigParseError.hpp"
#include "utils/errorUtils.hpp"
#include "utils/filesystemUtils.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <unordered_set>

namespace {
namespace fs = std::filesystem;

/**
 * @brief Ensures that each server has at least one location block.
 *
 * @details Iterates through all parsed servers and checks whether they
 *          contain at least one `Location` directive.
 *          A server without locations cannot properly handle requests,
 *          so this function throws a `ValidationError` if any server is
 *          missing location blocks.
 *
 * @param servers List of parsed server configurations to validate.
 *
 * @throws ValidationError If a server has no associated location blocks.
 *
 * @ingroup config_validation
 */
void validateHasLocation(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        if (servers[serverIndex].getLocations().empty()) {
            // Invalid: No location blocks defined for this server
            throw ValidationError("Missing location blocks in server #" +
                                  std::to_string(serverIndex + 1) +
                                  "\n→ Add at least one 'location' block to handle requests");
        }
    }
}

/**
 * @brief Validates location paths inside each server block.
 *
 * @details This function ensures that:
 *  - Every location path is non-empty and begins with a '/'.
 *  - No path segment starts with '.' (disallows '.', '..', hidden or malformed segments).
 *  - Each location path within a server is unique (no duplicates allowed).
 *
 * Violations result in a `ValidationError` with a detailed message.
 *
 * @param servers List of parsed server configurations to validate.
 *
 * @throws ValidationError If:
 *         - A location path is empty or does not start with '/',
 *         - A path segment begins with '.',
 *         - A duplicate location path is found within the same server.
 *
 * @ingroup config_validation
 */
void validateLocationPaths(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const Server&                   server = servers[serverIndex];
        std::unordered_set<std::string> seenPaths;

        for (const Location& loc : server.getLocations()) {
            const std::string& path = loc.getPath();

            // Must be non-empty and start with '/'
            if (path.empty() || path[0] != '/') {
                throw ValidationError("Invalid location path '" + path + "' in server #" +
                                      std::to_string(serverIndex + 1) + "\n→ Must start with '/'");
            }

            // Validate each segment of the path (no '.'-prefixed parts)
            fs::path locPath(path);
            for (const auto& part : locPath) {
                const std::string seg = part.string();
                if (!seg.empty() && seg[0] == '.') {
                    throw ValidationError("Invalid location path '" + path + "' in server #" +
                                          std::to_string(serverIndex + 1) +
                                          "\n→ Path segments must not begin with '.' (e.g. '.', "
                                          "'..', '...', '.hidden')");
                }
            }

            // Ensure path uniqueness within the same server
            if (!seenPaths.insert(path).second) {
                throw ValidationError("Duplicate location path '" + path + "' in server #" +
                                      std::to_string(serverIndex + 1));
            }
        }
    }
}

/**
 * @brief Validates default requirements for each location block.
 *
 * @details This function enforces two critical invariants for location definitions:
 *  - Each location must define at least one of the following:
 *    - A `root` directive (filesystem path for content),
 *    - OR a `return` directive (HTTP redirection).
 *  - A location cannot define both CGI behavior (`cgi_extension`) and a redirection
 *    (`return`) at the same time, since these are mutually exclusive.
 *
 * Violations result in a `ValidationError` with descriptive guidance.
 *
 * @param servers List of parsed server configurations to validate.
 *
 * @throws ValidationError If:
 *         - A location block has neither `root` nor `return`,
 *         - A location block has both `return` and `cgi_extension` defined.
 *
 * @ingroup config_validation
 */
void validateLocationDefaults(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const Server&                server    = servers[serverIndex];
        const std::vector<Location>& locations = server.getLocations();

        for (std::size_t locationIndex = 0; locationIndex < locations.size(); ++locationIndex) {
            const Location&    location = locations[locationIndex];
            const std::string& path     = location.getPath();

            // Must have either a root or a return directive
            if (location.getRoot().empty() && !location.hasRedirect()) {
                throw ValidationError(
                    "Location '" + path + "' in server #" + std::to_string(serverIndex + 1) +
                    " is missing both a 'root' and a 'return' directive" +
                    "\n→ Each location must have at least a 'root' or a 'return'");
            }

            // Cannot combine CGI behavior with redirect
            if (location.hasRedirect() && !location.getCgiExtensions().empty()) {
                throw ValidationError(
                    "Location '" + path + "' in server #" + std::to_string(serverIndex + 1) +
                    " defines both CGI behavior and a redirection" +
                    "\n→ A location cannot combine 'return' with 'cgi_extension'");
            }
        }
    }
}

/**
 * @brief Splits a domain name into its individual labels.
 *
 * @details A domain name like `"example.com"` is split into
 *          `{"example", "com"}`. The splitting uses `.` as the separator,
 *          and empty labels are preserved if consecutive dots appear.
 *
 * @param name Domain name string to split.
 * @return A vector of labels (substrings between dots).
 *
 * @ingroup config_validation
 */
static std::vector<std::string> splitLabels(const std::string& name) {
    std::vector<std::string> labels;
    std::size_t              start = 0;
    while (start < name.size()) {
        std::size_t end = name.find('.', start);
        if (end == std::string::npos)
            end = name.size();
        labels.push_back(name.substr(start, end - start));
        start = end + 1;
    }
    return labels;
}

/**
 * @brief Validates a single domain label according to RFC 1035.
 *
 * @details Checks that:
 *  - Label is non-empty and no longer than 63 characters.
 *  - Does not begin or end with a hyphen (`-`).
 *  - Contains only alphanumeric characters (`a-z`, `A-Z`, `0-9`) or hyphens.
 *
 * @param label The label string to validate.
 * @return `true` if the label is valid, `false` otherwise.
 *
 * @ingroup config_validation
 */
bool isValidLabel(const std::string& label) {
    if (label.empty() || label.size() > 63)
        return false;
    if (label.front() == '-' || label.back() == '-')
        return false;
    for (char c : label) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-')
            return false;
    }
    return true;
}

/**
 * @brief Validates a full server name against domain format rules.
 *
 * @details Enforces RFC 1035-style constraints:
 *  - Name must be non-empty and no longer than 253 characters.
 *  - Cannot contain consecutive dots (`..`), which would create empty labels.
 *  - Each label (between dots) must satisfy isValidLabel.
 *
 * @param name Full domain/server name to validate.
 * @return `true` if the name follows domain rules, `false` otherwise.
 *
 * @ingroup config_validation
 */
bool isServerNameValid(const std::string& name) {
    if (name.empty() || name.size() > 253)
        return false;

    if (name.find("..") != std::string::npos) // No empty labels
        return false;

    std::vector<std::string> labels = splitLabels(name);
    for (const std::string& label : labels) {
        if (!isValidLabel(label))
            return false;
    }

    return true;
}

/**
 * @brief Validates the format of all `server_name` directives across servers.
 *
 * @details Ensures that every declared `server_name` in the configuration
 *          adheres to domain naming conventions and does not contain invalid
 *          characters. Specifically, it checks:
 *
 *  - Non-empty names: Empty strings are not allowed.
 *  - Printable characters only: No control characters (e.g., newlines, tabs).
 *  - No whitespace: Spaces are not valid inside domain names.
 *  - RFC 1035 compliance via isServerNameValid:
 *      - Maximum length 253 characters.
 *      - No empty labels (e.g. `"example..com"`).
 *      - Labels up to 63 characters, only alphanumeric and `-`.
 *      - No leading or trailing `-` in a label.
 *
 * If any of these conditions are violated, a ValidationError is thrown
 * with a descriptive message indicating the invalid server name and the server
 * block index where it was found.
 *
 * @param servers Vector of configured Server objects to validate.
 *
 * @throws ValidationError if an invalid `server_name` is detected.
 *
 * @ingroup config_validation
 */
void validateServerNameFormat(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const std::vector<std::string>& serverNames = servers[serverIndex].getServerNames();
        for (std::size_t nameIndex = 0; nameIndex < serverNames.size(); ++nameIndex) {
            const std::string& name = serverNames[nameIndex];

            // must not be empty
            if (name.empty()) {
                throw ValidationError("Empty server_name is not allowed in server #" +
                                      std::to_string(serverIndex + 1) +
                                      "\n→ Specify a non-empty string for server_name");
            }

            // must contain only printable characters
            for (std::size_t charIndex = 0; charIndex < name.size(); ++charIndex) {
                unsigned char c = static_cast<unsigned char>(name[charIndex]);
                if (!std::isprint(c)) {
                    throw ValidationError(
                        "Invalid control character in server_name in server #" +
                        std::to_string(serverIndex + 1) + " at position " +
                        std::to_string(charIndex) + " of '" + name + "'" +
                        "\n→ Server names must not contain newlines, tabs, or control characters");
                }
            }

            // no whitespace allowed
            if (name.find(' ') != std::string::npos) {
                throw ValidationError(
                    "Whitespace not allowed in server_name in server #" +
                    std::to_string(serverIndex + 1) + ": '" + name +
                    "'\n→ Use valid domain-like names without spaces or line breaks");
            }

            // must follow domain name rules (RFC 1035)
            if (!isServerNameValid(name)) {
                throw ValidationError(
                    "Invalid domain format in server_name in server #" +
                    std::to_string(serverIndex + 1) + ": '" + name +
                    "'\n→ Must follow domain format (RFC 1035): labels may only contain a-z, 0-9, "
                    "dashes; no empty labels, no leading/trailing dashes, and max 253 characters "
                    "total");
            }
        }
    }
}

/**
 * @brief Ensures uniqueness of `server_name` directives within each server block.
 *
 * @details This function validates that no single `server` block declares the same
 *          `server_name` more than once. While multiple distinct names are allowed
 *          (for virtual hosting and aliases), each must be unique within the same
 *          server definition.
 *
 * Validation rules:
 *  - A `server_name` must not appear more than once in the same server block.
 *  - Duplicate names within the same block trigger a @ref ValidationError.
 *  - Cross-server duplicates are allowed if they bind to different host:port
 *    combinations (checked separately in @ref validateUniquePorts).
 *
 * @param servers Vector of configured Server objects to validate.
 *
 * @throws ValidationError if a duplicate `server_name` is found inside the
 *         same server block.
 *
 * @ingroup config_validation
 *
 * @see validateServerNameFormat   Validates syntax/format of each server_name.
 * @see validateUniquePorts        Ensures uniqueness of (host, port, server_name)
 *                                 tuples across servers.
 */
void validateUniqueServerNames(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const std::vector<std::string>& names = servers[serverIndex].getServerNames();
        std::unordered_set<std::string> seen;

        for (const std::string& name : names) {
            if (!seen.insert(name).second) {
                throw ValidationError("Duplicate server_name '" + name + "' found in server #" +
                                      std::to_string(serverIndex + 1) +
                                      "\n→ Each server block must declare unique names");
            }
        }
    }
}

/**
 * @brief Ensures uniqueness of (host, port, server_name) bindings across servers.
 *
 * @details This function validates that no two servers conflict on the same
 *          `(host, port, server_name)` tuple, which would otherwise cause
 *          ambiguous routing at runtime.
 *
 * Validation rules:
 *  - Each `(host, port, server_name)` combination must be unique globally.
 *  - If a server has no `server_name`, it is considered the default for that
 *    `(host, port)` pair. Only one default server is allowed per host:port.
 *  - Duplicate `server_name` values across different servers are allowed only
 *    if they bind to different `(host, port)` pairs.
 *
 * @param servers Vector of configured Server objects to validate.
 *
 * @throws ValidationError if:
 *   - Two or more servers define the same `(host, port, server_name)` combination.
 *   - More than one default server (no `server_name`) exists on the same host:port.
 *
 * @ingroup config_validation
 *
 * @see validateUniqueServerNames   Ensures names are unique inside one server block.
 * @see validateServerNameFormat    Checks domain-like syntax of each server_name.
 */
void validateUniquePorts(const std::vector<Server>& servers) {
    using HostPort = std::pair<std::string, int>;
    using NameSet  = std::unordered_set<std::string>;
    std::map<HostPort, NameSet> bindMap;

    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const Server& server  = servers[serverIndex];
        HostPort      key     = std::make_pair(server.getHost(), server.getPort());
        NameSet&      nameSet = bindMap[key];

        const std::vector<std::string>& names = server.getServerNames();

        if (names.empty()) {
            if (nameSet.count("")) {
                throw ValidationError(
                    "Duplicate default server on " + server.getHost() + ":" +
                    std::to_string(server.getPort()) +
                    "\n→ Only one unnamed (default) server is allowed per host:port combination");
            }
            nameSet.insert(""); // Default marker
        } else {
            for (const std::string& name : names) {
                if (!nameSet.insert(name).second) {
                    throw ValidationError(
                        "Duplicate server_name '" + name + "' on " + server.getHost() + ":" +
                        std::to_string(server.getPort()) +
                        "\n→ Each virtual host (host:port + server_name) must be unique");
                }
            }
        }
    }
}

/**
 * @brief Validates that all configured error_page directives use valid HTTP status codes.
 *
 * @details Iterates over each server’s `error_page` mappings and ensures that every
 *          status code falls within the HTTP error range **400–599**.
 *          These codes represent client (4xx) and server (5xx) errors only.
 *          Other codes (e.g., 200, 301) are not valid for error pages.
 *
 * @param servers Vector of configured Server objects to validate.
 *
 * @throws ValidationError if:
 *   - Any error_page status code is outside the 400–599 range.
 *
 * @ingroup config_validation
 */
void validateErrorPageCodes(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const Server&                     server     = servers[serverIndex];
        const std::map<int, std::string>& errorPages = server.getErrorPages();

        for (const auto& pair : errorPages) {
            int code = pair.first;

            if (code < 400 || code > 599) {
                throw ValidationError("Invalid HTTP error code in server #" +
                                      std::to_string(serverIndex + 1) + ": " +
                                      std::to_string(code) +
                                      "\n→ Valid error_page codes must be in the 400-599 range "
                                      "(client/server errors only)");
            }
        }
    }
}

/**
 * @brief Validates that all `return` directives use valid HTTP redirection codes.
 *
 * @details This function iterates through all locations of each server
 *          and checks the status codes defined in the `return` directive.
 *          Only the following redirection status codes are allowed:
 *          301, 302, 303, 307, 308.
 *
 *          These represent permanent or temporary redirects and are standard
 *          in HTTP/1.1. Any other code (e.g., 200, 404, 500) is invalid
 *          when used with `return`.
 *
 * @param servers Vector of configured Server objects to validate.
 *
 * @throws ValidationError if:
 *   - A `return` directive uses a status code not in {301, 302, 303, 307, 308}.
 *
 * @ingroup config_validation
 */
void validateRedirectCodes(const std::vector<Server>& servers) {
    static const std::array<int, 5> ALLOWED = {{301, 302, 303, 307, 308}};
    for (std::size_t i = 0; i < servers.size(); ++i) {
        for (const Location& loc : servers[i].getLocations()) {
            if (!loc.hasRedirect())
                continue;

            int code = loc.getReturnCode();
            if (std::find(ALLOWED.begin(), ALLOWED.end(), code) == ALLOWED.end()) {
                throw ValidationError(
                    "Invalid redirect code " + std::to_string(code) + " in location '" +
                    loc.getPath() + "' of server #" + std::to_string(i + 1) +
                    "\n→ Supported redirect codes are 301, 302, 303, 307 and 308");
            }
        }
    }
}

/**
 * @brief Validates that all HTTP methods declared in `methods` directives are allowed.
 *
 * @details This function iterates through every location of each server and
 *          ensures that the configured HTTP methods are valid.
 *          Only the following methods are supported by this server:
 *          **GET, POST, DELETE**.
 *
 *          If any other method (e.g., PUT, PATCH, OPTIONS, HEAD) is specified,
 *          the configuration is considered invalid and a ValidationError is thrown.
 *
 * @param servers Vector of configured Server objects to validate.
 *
 * @throws ValidationError if:
 *   - A location declares an unsupported HTTP method.
 *
 * @ingroup config_validation
 */
void validateAllowedMethods(const std::vector<Server>& servers) {
    static const std::set<std::string> validMethods = {"GET", "POST", "DELETE"};

    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const Server&                server    = servers[serverIndex];
        const std::vector<Location>& locations = server.getLocations();

        for (const Location& loc : locations) {
            for (const std::string& method : loc.getMethods()) {
                if (!validMethods.count(method)) {
                    throw ValidationError("Invalid HTTP method '" + method + "' in location '" +
                                          loc.getPath() + "' of server #" +
                                          std::to_string(serverIndex + 1) +
                                          "\n→ Allowed methods are: GET, POST, DELETE");
                }
            }
        }
    }
}

/**
 * @brief Validates that `client_max_body_size` is set to a positive value.
 *
 * @details This function checks each server’s configured maximum request body size
 *          (`client_max_body_size`) to ensure it is not set to **zero**.
 *          A value of `0` would effectively disable all request bodies,
 *          which is considered an invalid configuration.
 *
 *          The size is typically specified in bytes or with human-readable units
 *          like `k`, `m`, or `g` (e.g., `1m` = 1 megabyte).
 *
 * @param servers Vector of configured Server objects to validate.
 *
 * @throws ValidationError if:
 *   - `client_max_body_size` is set to `0`.
 *
 * @ingroup config_validation
 */
void validateClientMaxBodySize(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const Server& server = servers[serverIndex];
        std::size_t   size   = server.getClientMaxBodySize();

        if (size == 0) {
            throw ValidationError("Invalid client_max_body_size in server #" +
                                  std::to_string(serverIndex + 1) +
                                  "\n→ Zero disables request bodies; Set 'client_max_body_size' to "
                                  "a positive size like '1m' or '1024'");
        }
    }
}

/**
 * @brief Validates the format of CGI extensions in all server locations.
 *
 * @details Iterates over every server and its `location` blocks to ensure
 *          that configured CGI extensions follow a valid format:
 *          - Must **not** be empty.
 *          - Must begin with a dot (`.`).
 *          - Must contain at least one alphanumeric character after the dot
 *            (e.g., `.php`, `.py`, `.pl`).
 *
 *          This ensures that CGI extensions are properly declared before they
 *          are mapped to interpreters using the `cgi_interpreter` directive.
 *
 * @param servers Vector of configured Server objects to validate.
 *
 * @throws ValidationError if:
 *   - The extension is empty (`""`).
 *   - The extension is only `"."`.
 *   - The extension does not start with a dot (`.`).
 *
 * @ingroup config_validation
 */
void validateCgiExtensions(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const Server&                server    = servers[serverIndex];
        const std::vector<Location>& locations = server.getLocations();

        for (const Location& loc : locations) {
            const std::vector<std::string>& extensions = loc.getCgiExtensions();

            for (const std::string& ext : extensions) {
                if (ext.empty() || ext == "." || ext[0] != '.') {
                    throw ValidationError("Invalid CGI extension '" + ext + "' in location '" +
                                          loc.getPath() + "' of server #" +
                                          std::to_string(serverIndex + 1) +
                                          "\n→ CGI extensions must start with a dot and contain a "
                                          "valid name ('.php', '.py')");
                }
            }
        }
    }
}

/**
 * @brief Validates `index` directives in all server locations.
 *
 * @details Iterates over every server and its `location` blocks to ensure
 *          that any declared `index` directive is meaningful. An index file
 *          (e.g., `index.html`) is only valid if the location also specifies
 *          a `root` directory, since the server must resolve the index file
 *          path relative to that root.
 *
 * @param servers Vector of configured Server objects to validate.
 *
 * @throws ValidationError if:
 *   - A `location` declares an `index` directive but has no `root`.
 *
 * @ingroup config_validation
 */
void validateIndexFiles(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const Server&                server    = servers[serverIndex];
        const std::vector<Location>& locations = server.getLocations();

        for (const Location& loc : locations) {
            const std::string& path  = loc.getPath();
            const std::string& root  = loc.getRoot();
            const std::string& index = loc.getIndex();

            if (!index.empty() && root.empty()) {
                throw ValidationError(
                    "Location '" + path + "' in server #" + std::to_string(serverIndex + 1) +
                    " defines an 'index' without a 'root'" +
                    "\n→ The 'index' directive requires a valid 'root' to resolve file paths");
            }
        }
    }
}

/* void validateAbsolutePaths(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const Server& server = servers[serverIndex];

        // --- error_page paths
        for (const auto& [code, pathStr] : server.getErrorPages()) {
            if (isInvalidAbsolutePath(pathStr)) {
                throw ValidationError(
                    "Invalid error_page path '" + pathStr + "' in server #" +
                    std::to_string(serverIndex + 1) +
                    "\n→ Must be a normalized absolute path with no '..' or '//' segments");
            }
        }

        for (const Location& loc : server.getLocations()) {
            const std::string& locationPath = loc.getPath();

            // --- Root
            const std::string& rootStr = loc.getRoot();
            if (!rootStr.empty() && isInvalidAbsolutePath(rootStr)) {
                throw ValidationError(
                    "Invalid root path '" + rootStr + "' in location '" + locationPath +
                    "' of server #" + std::to_string(serverIndex + 1) +
                    "\n→ Must be a normalized absolute path with no '..' or '//' segments");
            }

            // --- Upload store
            if (loc.isUploadEnabled()) {
                const std::string& uploadStr = loc.getUploadStore();
                if (isInvalidAbsolutePath(uploadStr)) {
                    throw ValidationError(
                        "Invalid upload_store path '" + uploadStr + "' in location '" +
                        locationPath + "' of server #" + std::to_string(serverIndex + 1) +
                        "\n→ Must be a normalized absolute path with no '..' or '//' segments");
                }
            }

            // --- Index files
            const std::vector<std::string>& indices = loc.getIndexFiles();
            for (const std::string& index : indices) {
                if (!index.empty() && isSuspiciousFilename(index)) {
                    throw ValidationError(
                        "Invalid index file '" + index + "' in location '" + locationPath +
                        "' of server #" + std::to_string(serverIndex + 1) +
                        "\n→ Must be a clean filename (no '/', '..', or special characters)");
                }
            }
        }
    }
} */

/**
 * @brief Validates consistency between `cgi_extension` and `cgi_interpreter` directives.
 *
 * @details Ensures that every declared CGI extension has a corresponding
 *          interpreter and that no extra interpreters exist for undeclared
 *          extensions. This prevents misconfigured mappings where requests
 *          could not be executed correctly or could point to unused binaries.
 *
 * @param servers Vector of configured Server objects to validate.
 *
 * @throws ValidationError if:
 *   - A `cgi_extension` exists without a matching `cgi_interpreter`.
 *   - A `cgi_interpreter` is defined for an extension that is not listed in
 *     `cgi_extension` (orphaned mapping).
 *
 * @ingroup config_validation
 */
void validateCgiInterpreters(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const Server&                server    = servers[serverIndex];
        const std::vector<Location>& locations = server.getLocations();

        for (const Location& loc : locations) {
            const std::string&              locationPath = loc.getPath();
            const std::vector<std::string>& exts         = loc.getCgiExtensions();

            for (const std::string& ext : exts) {
                std::string interp = loc.getCgiInterpreter(ext);
                if (interp.empty()) {
                    throw ValidationError("Missing cgi_interpreter for extension '" + ext +
                                          "' in location '" + locationPath + "' of server #" +
                                          std::to_string(serverIndex + 1) +
                                          "\n→ Each extension in 'cgi_extension' must be mapped to "
                                          "an interpreter with 'cgi_interpreter'");
                }
            }

            const std::map<std::string, std::string>& allInterps = loc.getCgiInterpreterMap();
            for (std::map<std::string, std::string>::const_iterator it = allInterps.begin();
                 it != allInterps.end(); ++it) {
                if (std::find(exts.begin(), exts.end(), it->first) == exts.end()) {
                    throw ValidationError(
                        "Orphan cgi_interpreter defined for extension '" + it->first +
                        "' in location '" + locationPath + "' of server #" +
                        std::to_string(serverIndex + 1) +
                        "\n→ Every 'cgi_interpreter' must be listed in 'cgi_extension'. "
                        "Remove the extra mapping or declare the extension.");
                }
            }
        }
    }
}

/**
 * @brief Validates that all `root` directives point to existing directories.
 *
 * @details Iterates over all server locations and verifies that:
 *   - Locations without a `return` directive must have a `root`.
 *   - The specified root path must exist on the filesystem.
 *   - The path must be a directory (not a file or invalid path).
 *
 * This ensures that each location serving files has a valid, accessible root
 * directory, preventing runtime errors due to missing or invalid paths.
 *
 * @param servers Vector of configured Server objects to validate.
 *
 * @throws ValidationError if:
 *   - A location missing a `return` directive has no `root`.
 *   - The specified root path does not exist.
 *   - The root path exists but is not a directory.
 *
 * @ingroup config_validation
 */
void validateRootsExist(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const Server& server = servers[serverIndex];

        for (const Location& loc : server.getLocations()) {
            // Redirect-only locations do not require a filesystem root
            if (loc.hasRedirect())
                continue;

            const std::string& root = loc.getRoot();

            // Defensive check: non-redirecting locations must define a root
            // (Should already be enforced by validateLocationDefaults)
            if (root.empty()) {
                throw ValidationError("Location '" + loc.getPath() + "' in server #" +
                                      std::to_string(serverIndex + 1) +
                                      " is missing a 'root' (required when no 'return' is set)");
            }

            // Probe filesystem status; capture errors via std::error_code
            std::error_code       ec;
            const fs::file_status st = fs::status(root, ec);

            // Path must exist
            if (ec || !fs::exists(st)) {
                throw ValidationError("Root path '" + root + "' does not exist for location '" +
                                      loc.getPath() + "' in server #" +
                                      std::to_string(serverIndex + 1) +
                                      "\n→ Create the directory or update the 'root' path.");
            }

            // Path must be a directory (not a regular file/symlink/etc.)
            if (!fs::is_directory(st)) {
                throw ValidationError("Root path '" + root + "' is not a directory for location '" +
                                      loc.getPath() + "' in server #" +
                                      std::to_string(serverIndex + 1) +
                                      "\n→ Point 'root' to an existing directory.");
            }
        }
    }
}

/**
 * @brief Validates upload store directories for all servers and locations.
 *
 * @details Ensures that whenever `upload_store` is enabled in a location block:
 *          - The configured path exists in the filesystem.
 *          - The path points to a directory (not a file or special node).
 *
 * @param servers Collection of server configuration objects to validate.
 *
 * @throws ValidationError If:
 *         - The configured upload store path does not exist.
 *         - The path exists but is not a directory.
 *
 * @ingroup config_validation
 */
void validateUploadStores(const std::vector<Server>& servers) {
    namespace fs = std::filesystem;

    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const Server& server = servers[serverIndex];

        for (const Location& loc : server.getLocations()) {
            if (!loc.isUploadEnabled())
                continue;

            const std::string& uploadStr    = loc.getUploadStore();
            const std::string& locationPath = loc.getPath();

            std::error_code ec;
            fs::file_status st = fs::status(uploadStr, ec);

            if (ec || !fs::exists(st)) {
                throw ValidationError("Upload store '" + uploadStr +
                                      "' does not exist in location '" + locationPath +
                                      "' of server #" + std::to_string(serverIndex + 1) +
                                      "\n→ Create the directory or update your configuration");
            }

            if (!fs::is_directory(st)) {
                throw ValidationError("Upload store '" + uploadStr +
                                      "' is not a directory in location '" + locationPath +
                                      "' of server #" + std::to_string(serverIndex + 1) +
                                      "\n→ Ensure it points to a valid directory");
            }
        }
    }
}

} // namespace

void validateConfig(const Config& config) {
    const auto& servers = config.getServers();
    validateHasLocation(servers);
    validateLocationPaths(servers);
    validateUniquePorts(servers);
    validateServerNameFormat(servers);
    validateUniqueServerNames(servers);
    validateLocationDefaults(servers);
    validateErrorPageCodes(servers);
    validateRedirectCodes(servers);
    validateAllowedMethods(servers);
    validateClientMaxBodySize(servers);
    validateCgiExtensions(servers);
    validateIndexFiles(servers);
    // validateAbsolutePaths(servers);
    validateCgiInterpreters(servers);
    validateUploadStores(servers);
    validateRootsExist(servers);
}
