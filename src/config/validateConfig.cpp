/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   validateConfig.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/20 23:23:50 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/25 22:30:44 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/validateConfig.hpp"
#include "config/parser/ConfigParseError.hpp"
#include "utils/errorUtils.hpp"
#include "utils/filesystemUtils.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_set>

namespace fs = std::filesystem;

namespace {
void validateHasLocation(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        if (servers[serverIndex].getLocations().empty()) {
            throw ValidationError("Missing location blocks in server #" +
                                  std::to_string(serverIndex + 1) +
                                  "\n→ Add at least one 'location' block to handle requests");
        }
    }
}

void validateLocationPaths(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const Server&                   server = servers[serverIndex];
        std::unordered_set<std::string> seenPaths;

        for (const Location& loc : server.getLocations()) {
            const std::string& path = loc.getPath();

            if (path.empty() || path[0] != '/') {
                throw ValidationError("Invalid location path '" + path + "' in server #" +
                                      std::to_string(serverIndex + 1) + "\n→ Must start with '/'");
            }

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

            if (!seenPaths.insert(path).second) {
                throw ValidationError("Duplicate location path '" + path + "' in server #" +
                                      std::to_string(serverIndex + 1));
            }
        }
    }
}

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

            // Cannot combine CGI behavior and redirect
            if (location.hasRedirect() && !location.getCgiExtensions().empty()) {
                throw ValidationError(
                    "Location '" + path + "' in server #" + std::to_string(serverIndex + 1) +
                    " defines both CGI behavior and a redirection" +
                    "\n→ A location cannot combine 'return' with 'cgi_extension'");
            }
        }
    }
}

// Utility: Split string by '.'
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

// Validate a single label (RFC 1035 rules)
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

// Full domain validation
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

void validateServerNameFormat(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const std::vector<std::string>& serverNames = servers[serverIndex].getServerNames();
        for (std::size_t nameIndex = 0; nameIndex < serverNames.size(); ++nameIndex) {
            const std::string& name = serverNames[nameIndex];

            if (name.empty()) {
                throw ValidationError("Empty server_name is not allowed in server #" +
                                      std::to_string(serverIndex + 1) +
                                      "\n→ Specify a non-empty string for server_name");
            }

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

            if (name.find(' ') != std::string::npos) {
                throw ValidationError(
                    "Whitespace not allowed in server_name in server #" +
                    std::to_string(serverIndex + 1) + ": '" + name +
                    "'\n→ Use valid domain-like names without spaces or line breaks");
            }

            if (!isServerNameValid(name)) {
                throw ValidationError(
                    "Invalid domain format in server_name in server #" +
                    std::to_string(serverIndex + 1) + ": '" + name +
                    "'\n→ Must follow domain format (RFC 1035): labels may only contain a-z, 0-9, "
                    "dashes; "
                    "no empty labels, no leading/trailing dashes, and max 253 characters total");
            }
        }
    }
}

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

void validateRedirectCodes(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const std::vector<Location>& locations = servers[serverIndex].getLocations();

        for (const Location& loc : locations) {
            if (loc.hasRedirect()) {
                int code = loc.getReturnCode();

                if (code < 300 || code > 399) {
                    throw ValidationError("Invalid redirect code " + std::to_string(code) +
                                          " in location '" + loc.getPath() + "' of server #" +
                                          std::to_string(serverIndex + 1) +
                                          "\n→ Redirect codes must be between 300 and 399 (301 for "
                                          "permanent, 302 for temporary)");
                }
            }
        }
    }
}

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

void validateAbsolutePaths(const std::vector<Server>& servers) {
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

            /* // --- Index files /// !!!!!!!!!!!!!!!!!!!!!!!
            const std::vector<std::string>& indices = loc.getIndexFiles();
            for (const std::string& index : indices) {
                if (!index.empty() && isSuspiciousFilename(index)) {
                    throw ValidationError(
                        "Invalid index file '" + index + "' in location '" + locationPath +
                        "' of server #" + std::to_string(serverIndex + 1) +
                        "\n→ Must be a clean filename (no '/', '..', or special characters)");
                }
            } */
        }
    }
}

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

/* void validateFilesystem(const std::vector<Server>& servers) {
    for (std::size_t serverIndex = 0; serverIndex < servers.size(); ++serverIndex) {
        const Server& server = servers[serverIndex];

        for (const Location& loc : server.getLocations()) {
            const std::string& locationPath = loc.getPath();

            // --- Root
            const std::string& rootStr = loc.getRoot();
            if (!rootStr.empty()) {
                if (!fs::exists(rootStr)) {
                    throw ValidationError("Root path '" + rootStr +
                                          "' does not exist in location '" + locationPath +
                                          "' of server #" + std::to_string(serverIndex + 1));
                }
                if (!fs::is_directory(rootStr)) {
                    throw ValidationError("Root path '" + rootStr +
                                          "' is not a directory in location '" + locationPath +
                                          "' of server #" + std::to_string(serverIndex + 1));
                }
            }

            // --- Upload Store
            if (loc.isUploadEnabled()) {
                const std::string& uploadStr = loc.getUploadStore();
                if (!fs::exists(uploadStr)) {
                    throw ValidationError("Upload store '" + uploadStr +
                                          "' does not exist in location '" + locationPath +
                                          "' of server #" + std::to_string(serverIndex + 1));
                }
                if (!fs::is_directory(uploadStr)) {
                    throw ValidationError("Upload store '" + uploadStr +
                                          "' is not a directory in location '" + locationPath +
                                          "' of server #" + std::to_string(serverIndex + 1));
                }
            }
        }
    }
} */

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
    validateAbsolutePaths(servers);
    validateCgiInterpreters(servers);
    /* validateFilesystem(servers); */
}
