/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   test_config.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/09 09:46:03 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/09 09:46:04 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/Config.hpp"
#include "core/Server.hpp"
#include <cassert>
#include <iostream>

void test_default_is_empty() {
    Config config;
    assert(config.getServers().empty());
}

void test_add_server() {
    Config config;
    Server server;
    config.addServer(server);
    assert(config.getServers().size() == 1);
}

void test_get_servers_reference() {
    Config config;
    Server server;
    config.addServer(server);

    const auto& servers_ref = config.getServers();
    // Ensure getServers() returns a reference (not a copy)
    assert(&servers_ref == &config.getServers());
    assert(servers_ref.size() == 1);
}

void test_move_constructor() {
    Config original;
    original.addServer(Server());

    Config moved(std::move(original));
    assert(moved.getServers().size() == 1);
    assert(original.getServers().empty()); // Valid moved-from object must be empty
}

void test_move_assignment() {
    Config original;
    original.addServer(Server());

    Config moved;
    moved = std::move(original);
    assert(moved.getServers().size() == 1);
    assert(original.getServers().empty());
}

int main() {
    test_default_is_empty();
    test_add_server();
    test_get_servers_reference();
    test_move_constructor();
    test_move_assignment();

    std::cout << "✅ All Config tests passed successfully.\n";
    return 0;
}
