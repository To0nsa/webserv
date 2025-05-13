/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigValidator.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/07 20:00:00 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/11 23:11:28 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/validator/ConfigValidator.hpp"
#include "config/parser/ConfigParseError.hpp"
#include "utils/errorUtils.hpp"

#include <iostream>
#include <string>
#include <unordered_set>

void ConfigValidator::validate(const Config& config) {
    const auto& servers = config.getServers();
    validateHasLocation(servers);
    validateUniquePorts(servers);
    validateUniqueServerNames(servers);
    validateLocationDefaults(servers);
    validateErrorPageCodes(servers);
    validateRedirectCodes(servers);
    validateAllowedMethods(servers);
    validateClientMaxBodySize(servers);
    validateUploadStorePaths(servers);
    validateCgiExtensions(servers);
}

void ConfigValidator::validateHasLocation(const std::vector<Server>& servers) {
    // Iterate over each server block in the configuration
    for (std::size_t server_index = 0; server_index < servers.size(); ++server_index) {
        // Check if the current server has no location blocks defined
        if (servers[server_index].getLocations().empty()) {
            // Throw a syntax error indicating the server is incomplete
            throw SyntaxError(
                formatError("Server #" + std::to_string(server_index) + " has no location blocks",
                            0, 0),
                "Add at least one 'location' block");
        }
    }
}

void ConfigValidator::validateUniqueServerNames(const std::vector<Server>& servers) {
    // Iterate over each server block
    for (std::size_t si = 0; si < servers.size(); ++si) {
        const auto&                     names = servers[si].getServerNames();
        std::unordered_set<std::string> seen; // Tracks encountered server names in this block

        // Check for duplicates within the current server block
        for (const std::string& name : names) {
            // If insertion fails, it's a duplicate
            if (!seen.insert(name).second) {
                throw SyntaxError(formatError("Duplicate server_name '" + name + "' in server #" +
                                                  std::to_string(si),
                                              0, 0),
                                  "Ensure each server_name is specified only once");
            }
        }
    }
}

void ConfigValidator::validateUniquePorts(const std::vector<Server>& servers) {
    using HostPort = std::pair<std::string, int>;     // Combines host + port as a key
    using NameSet  = std::unordered_set<std::string>; // Tracks server_names for a given socket
    std::map<HostPort, NameSet> bindMap;              // Maps each (host, port) to its claimed names

    // Iterate through all server blocks
    for (std::size_t i = 0; i < servers.size(); ++i) {
        const Server& server  = servers[i];
        HostPort      key     = std::make_pair(server.getHost(), server.getPort());
        NameSet&      nameSet = bindMap[key]; // Get or create the set of names for this host:port

        const std::vector<std::string>& names = server.getServerNames();

        if (names.empty()) {
            // No server_name means this server is the default for this host:port
            if (nameSet.count("")) {
                throw SyntaxError(formatError("Duplicate default server on " + server.getHost() +
                                                  ":" + std::to_string(server.getPort()),
                                              0, 0),
                                  "Only one unnamed server is allowed per host:port");
            }
            nameSet.insert(""); // Track default presence using empty string
        } else {
            for (const std::string& name : names) {
                // If insertion fails, the server_name was already declared for this host:port
                if (!nameSet.insert(name).second) {
                    throw SyntaxError(formatError("Duplicate server_name '" + name + "' on " +
                                                      server.getHost() + ":" +
                                                      std::to_string(server.getPort()),
                                                  0, 0),
                                      "Each virtual host (host:port + server_name) must be unique");
                }
            }
        }
    }
}

void ConfigValidator::validateLocationDefaults(const std::vector<Server>& servers) {
    // Loop over all servers
    for (std::size_t i = 0; i < servers.size(); ++i) {
        const Server&                server    = servers[i];
        const std::vector<Location>& locations = server.getLocations();

        // Check each location block within the server
        for (std::size_t j = 0; j < locations.size(); ++j) {
            const Location&    loc  = locations[j];
            const std::string& path = loc.getPath();

            // --- Rule 1: must have either a root or a return directive
            if (loc.getRoot().empty() && !loc.hasRedirect()) {
                throw SyntaxError(formatError("Location '" + path + "' in server #" +
                                                  std::to_string(i) +
                                                  " is missing both root and return",
                                              0, 0),
                                  "Each location must have either 'root' or 'return' directive");
            }

            // --- Rule 2: cannot combine redirect and CGI behavior
            if (loc.hasRedirect() && !loc.getCgiExtensions().empty()) {
                throw SyntaxError(formatError("Location '" + path + "' in server #" +
                                                  std::to_string(i) + " has both CGI and redirect",
                                              0, 0),
                                  "Cannot combine 'return' with 'cgi_extension'");
            }

            // --- Rule 3: methods must be declared (enforced even if normalizer fills them)
            if (loc.getMethods().empty()) {
                throw SyntaxError(formatError("Location '" + path + "' in server #" +
                                                  std::to_string(i) + " has no allowed methods",
                                              0, 0),
                                  "Use 'methods' directive to declare allowed HTTP verbs");
            }
        }
    }
}

void ConfigValidator::validateErrorPageCodes(const std::vector<Server>& servers) {
    // Loop over each server
    for (std::size_t i = 0; i < servers.size(); ++i) {
        const Server&                     server     = servers[i];
        const std::map<int, std::string>& errorPages = server.getErrorPages();

        // Check each error_page directive
        for (const auto& pair : errorPages) {
            int code = pair.first;

            // Valid HTTP error codes must be in the 400–599 range
            if (code < 400 || code > 599) {
                throw SyntaxError(formatError("Invalid HTTP error code in server #" +
                                                  std::to_string(i) + ": " + std::to_string(code),
                                              0, 0),
                                  "Valid error_page codes must be between 400 and 599");
            }
        }
    }
}

void ConfigValidator::validateRedirectCodes(const std::vector<Server>& servers) {
    // Iterate over each server
    for (std::size_t si = 0; si < servers.size(); ++si) {
        const auto& locations = servers[si].getLocations();

        // Check each location block within the server
        for (const Location& loc : locations) {
            // If a redirect is configured, validate its status code
            if (loc.hasRedirect()) {
                int code = loc.getReturnCode();

                // Redirect codes must fall within the 3xx range (per HTTP standard)
                if (code < 300 || code > 399) {
                    throw SyntaxError(formatError("Invalid redirect code " + std::to_string(code) +
                                                      " in location '" + loc.getPath() +
                                                      "' of server #" + std::to_string(si),
                                                  0, 0),
                                      "Redirect codes must be between 300 and 399");
                }
            }
        }
    }
}

void ConfigValidator::validateAllowedMethods(const std::vector<Server>& servers) {
    // Set of all valid HTTP methods (based on RFC 7231 and common extensions)
    static const std::set<std::string> valid_methods = {
        "GET", /* "HEAD", */ "POST",
        /* "PUT", */ "DELETE" /* , "CONNECT", "OPTIONS", "TRACE", "PATCH" */};

    // Iterate over all servers
    for (std::size_t i = 0; i < servers.size(); ++i) {
        const Server&                server    = servers[i];
        const std::vector<Location>& locations = server.getLocations();

        // Iterate over each location block
        for (std::size_t j = 0; j < locations.size(); ++j) {
            const Location& loc = locations[j];

            // Validate each method declared in the location
            for (const std::string& method : loc.getMethods()) {
                if (!valid_methods.count(method)) {
                    throw SyntaxError(formatError("Invalid HTTP method '" + method +
                                                      "' in location '" + loc.getPath() +
                                                      "' of server #" + std::to_string(i),
                                                  0, 0),
                                      "Allowed methods must be standard HTTP verbs");
                }
            }
        }
    }
}

void ConfigValidator::validateClientMaxBodySize(const std::vector<Server>& servers) {
    // Iterate through all servers
    for (std::size_t i = 0; i < servers.size(); ++i) {
        const Server& server = servers[i];
        std::size_t   size   = server.getClientMaxBodySize();

        // The body size must be strictly positive
        if (size == 0) {
            throw SyntaxError(formatError("Invalid client_max_body_size in server #" +
                                              std::to_string(i) + ": must be > 0",
                                          0, 0),
                              "Set 'client_max_body_size' to a positive size like '1m'");
        }
    }
}

void ConfigValidator::validateUploadStorePaths(const std::vector<Server>& servers) {
    // Iterate through all servers
    for (std::size_t i = 0; i < servers.size(); ++i) {
        const Server&                server    = servers[i];
        const std::vector<Location>& locations = server.getLocations();

        // Check each location block
        for (std::size_t j = 0; j < locations.size(); ++j) {
            const Location& loc = locations[j];

            // Skip locations that don't use upload_store
            if (!loc.isUploadEnabled()) {
                continue;
            }

            const std::string& path = loc.getUploadStore();

            // Check: path must not be empty
            if (path.empty()) {
                throw SyntaxError(formatError("Empty upload_store path in location '" +
                                                  loc.getPath() + "' of server #" +
                                                  std::to_string(i),
                                              0, 0),
                                  "Specify a directory path in 'upload_store'");
            }

            // Check: path must be absolute
            if (path[0] != '/') {
                throw SyntaxError(formatError("Relative upload_store path in location '" +
                                                  loc.getPath() + "' of server #" +
                                                  std::to_string(i),
                                              0, 0),
                                  "Upload store paths must be absolute (start with '/')");
            }

            // Check: no directory traversal allowed
            if (path.find("..") != std::string::npos) {
                throw SyntaxError(formatError("Invalid upload_store path in location '" +
                                                  loc.getPath() + "' of server #" +
                                                  std::to_string(i),
                                              0, 0),
                                  "Directory traversal is not allowed in upload_store paths");
            }
        }
    }
}

void ConfigValidator::validateCgiExtensions(const std::vector<Server>& servers) {
    // Iterate through all servers
    for (std::size_t i = 0; i < servers.size(); ++i) {
        const Server&                server    = servers[i];
        const std::vector<Location>& locations = server.getLocations();

        // Check each location block
        for (std::size_t j = 0; j < locations.size(); ++j) {
            const Location&                 loc        = locations[j];
            const std::vector<std::string>& extensions = loc.getCgiExtensions();

            // Validate each CGI extension
            for (const std::string& ext : extensions) {
                // Must be non-empty, not just ".", and start with a dot
                if (ext.empty() || ext == "." || ext[0] != '.') {
                    throw SyntaxError(formatError("Invalid CGI extension '" + ext +
                                                      "' in location '" + loc.getPath() +
                                                      "' of server #" + std::to_string(i),
                                                  0, 0),
                                      "CGI extensions must start with a dot (e.g., '.php', '.py')");
                }
            }
        }
    }
}
