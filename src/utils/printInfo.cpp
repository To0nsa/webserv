/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   printInfo.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 14:01:22 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/20 23:31:21 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "utils/printInfo.hpp"
#include "utils/stringUtils.hpp"

#include <sstream>

std::string printUsage() {
    std::ostringstream oss;
    oss << "========= USAGE =========\n"
        << "./webserv              # Uses default.conf\n"
        << "./webserv config.conf  # Uses custom config file\n";
    return oss.str();
}

void printServerHeader(int index, const Server& server) {
    std::cout << "\n"
              << "┌────────────────────────────────────────────────────────────┐\n"
              << "│ Server " << index + 1 << ": ";
    if (!server.getServerNames().empty())
        std::cout << server.getServerNames().front();
    else
        std::cout << "(no name)";
    std::cout << " (" << server.getHost() << ":" << server.getPort() << ")\n";
    std::cout << "└────────────────────────────────────────────────────────────┘\n";
}

void printConfig(Config& config) {
    const std::vector<Server>& servers = config.getServers();
    for (size_t i = 0; i < servers.size(); ++i) {
        const Server& server = servers[i];
        printServerHeader(i, server);

        std::cout << "  ▸ client_max_body_size : " << formatBytes(server.getClientMaxBodySize())
                  << "\n";

        const auto& errors = server.getErrorPages();
        if (!errors.empty()) {
            std::cout << "  ▸ error_pages:\n";
            for (std::map<int, std::string>::const_iterator it = errors.begin(); it != errors.end();
                 ++it) {
                std::cout << "      - " << it->first << " → " << it->second << "\n";
            }
        }

        const std::vector<Location>& locations = server.getLocations();
        for (size_t k = 0; k < locations.size(); ++k) {
            const Location& loc = locations[k];

            std::cout << "  ▸ location " << loc.getPath() << ":\n";
            std::cout << "      - root        : " << loc.getRoot() << "\n";
            std::cout << "      - index       : " << loc.getIndex() << "\n";
            std::cout << "      - autoindex   : " << (loc.isAutoindexEnabled() ? "on" : "off")
                      << "\n";

            const std::set<std::string>& methods = loc.getMethods();
            std::cout << "      - methods     : ";
            if (methods.empty())
                std::cout << "(none)";
            else {
                std::set<std::string>::const_iterator it = methods.begin();
                std::cout << *it;
                for (++it; it != methods.end(); ++it)
                    std::cout << ", " << *it;
            }
            std::cout << "\n";

            if (loc.hasRedirect()) {
                std::cout << "      - redirect    : " << loc.getRedirect();
                if (loc.getReturnCode())
                    std::cout << " (code " << loc.getReturnCode() << ")";
                std::cout << "\n";
            }

            if (loc.isUploadEnabled()) {
                std::cout << "      - upload_store: " << loc.getUploadStore() << "\n";
            }

            const std::vector<std::string>& cgi = loc.getCgiExtensions();
            if (!cgi.empty()) {
                std::cout << "      - cgi_pass    : ";
                for (size_t ci = 0; ci < cgi.size(); ++ci) {
                    std::cout << cgi[ci];
                    if (ci + 1 < cgi.size())
                        std::cout << ", ";
                }
                std::cout << "\n\n";
            }
        }
    }
}
