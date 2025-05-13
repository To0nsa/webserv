/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   test_server.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/09 09:43:09 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/09 09:44:59 by nlouis           ###   ########.fr       */
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
    assert(s.getClientMaxBodySize() == 1048576); // 1 MiB
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
    loc.setRoot("/srv/api");
    loc.setAutoindex(true);
    loc.addMethod("GET");
    s.addLocation(loc);

    assert(s.getPort() == 8080);
    assert(s.getHost() == "127.0.0.1");
    assert(s.getClientMaxBodySize() == 4096);

    const auto& names = s.getServerNames();
    assert(names.size() == 2);
    assert(names[0] == "localhost");
    assert(names[1] == "example.com");

    const auto& errors = s.getErrorPages();
    assert(errors.size() == 2);
    assert(errors.at(404) == "/errors/404.html");
    assert(errors.at(500) == "/errors/500.html");

    const auto& locs = s.getLocations();
    assert(locs.size() == 1);
    assert(locs[0].getPath() == "/api");
    assert(locs[0].getRoot() == "/srv/api");
    assert(locs[0].isAutoindexEnabled());
    assert(locs[0].isMethodAllowed("GET"));
}

void test_has_server_name() {
    Server s;
    s.addServerName("localhost");
    s.addServerName("example.com");

    assert(s.hasServerName("localhost"));
    assert(s.hasServerName("example.com"));
    assert(!s.hasServerName("unknown.com"));
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

    Server match1 = findMatchingServer(servers, 80, "example.com");
    assert(match1.hasServerName("example.com"));

    Server match2 = findMatchingServer(servers, 80, "unknown.com");
    assert(match2.hasServerName("localhost"));

    Server match3 = findMatchingServer(servers, 8080, "alternate.dev");
    assert(match3.hasServerName("alternate.dev"));

    Server match4 = findMatchingServer(servers, 8080, "unmatched.dev");
    assert(match4.getPort() == 8080);

    try {
        findMatchingServer(servers, 9999, "irrelevant");
        assert(false); // Should not reach here
    } catch (const std::runtime_error& e) {
        assert(std::string(e.what()).find("No matching server") != std::string::npos);
    }
}

int main() {
    test_default_values();
    test_setters_and_getters();
    test_has_server_name();
    test_find_matching_server();

    std::cout << "✅ All Server tests passed successfully.\n";
    return 0;
}
