/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   test_server.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/02 18:46:52 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/02 22:07:44 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/Server.hpp"
#include "core/server_utils.hpp"
#include <cassert>
#include <iostream>

void test_default_values() {
    Server s;
    assert(s.getPort() == 80);
    assert(s.getHost() == "0.0.0.0");
    assert(s.getClientMaxBodySize() == 1048576);
    assert(s.getServerNames().empty());
    assert(s.getErrorPages().empty());
    assert(s.getLocations().empty());
}

void test_setters_and_getters() {
    Server s;

    s.setPort(8080);
    s.setHost("127.0.0.1");
    s.setClientMaxBodySize(4096);
    s.addServerName("localhost");
    s.addServerName("example.com");
    s.setErrorPage(404, "/errors/404.html");
    s.setErrorPage(500, "/errors/500.html");

    Location loc;
    loc.setPath("/api");
    s.addLocation(loc);

    assert(s.getPort() == 8080);
    assert(s.getHost() == "127.0.0.1");
    assert(s.getClientMaxBodySize() == 4096);

    const std::vector<std::string>& names = s.getServerNames();
    assert(names.size() == 2);
    assert(names[0] == "localhost");
    assert(names[1] == "example.com");

    const std::map<int, std::string>& errors = s.getErrorPages();
    assert(errors.size() == 2);
    assert(errors.at(404) == "/errors/404.html");
    assert(errors.at(500) == "/errors/500.html");

    const std::vector<Location>& locs = s.getLocations();
    assert(locs.size() == 1);
    assert(locs[0].getPath() == "/api");
}

void test_has_server_name() {
    Server s;
    s.addServerName("localhost");
    s.addServerName("example.com");

    assert(s.hasServerName("localhost"));    // direct match
    assert(s.hasServerName("example.com"));  // second entry
    assert(!s.hasServerName("unknown.com")); // should not match
}

void test_find_matching_server() {
    std::vector<Server> servers;

    Server s1;
    s1.setPort(80);
    s1.addServerName("localhost");

    Server s2;
    s2.setPort(80);
    s2.addServerName("example.com");

    Server s3;
    s3.setPort(8080);
    s3.addServerName("alternate.dev");

    servers.push_back(s1);
    servers.push_back(s2);
    servers.push_back(s3);

    std::string name1 = "example.com";
    std::string name2 = "unknown.com";
    std::string name3 = "alternate.dev";
    std::string name4 = "unmatched";

    // Exact match on port and server name
    const Server& match1 = findMatchingServer(servers, 80, name1);
    assert(match1.hasServerName("example.com"));

    // Fallback match (no server_name match, fallback to first on port 80)
    const Server& match2 = findMatchingServer(servers, 80, name2);
    assert(match2.hasServerName("localhost"));

    // Exact match on different port
    const Server& match3 = findMatchingServer(servers, 8080, name3);
    assert(match3.hasServerName("alternate.dev"));

    // Match fallback by port only
    const Server& match4 = findMatchingServer(servers, 8080, name4);
    assert(match4.getPort() == 8080);

    try {
        findMatchingServer(servers, 9999, "any.com");
        assert(false); // should not reach here
    } catch (const std::runtime_error& e) {
        assert(std::string(e.what()).find("No matching server") != std::string::npos);
    }
}

int main() {
    test_default_values();
    test_setters_and_getters();
    test_has_server_name();

    std::cout << "✅ All Server tests passed successfully.\n";
    return 0;
}
