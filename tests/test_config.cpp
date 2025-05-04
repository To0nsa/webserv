/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   test_config.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/02 23:55:51 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/02 23:55:59 by nlouis           ###   ########.fr       */
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
    Server s;
    config.addServer(s);
    assert(config.getServers().size() == 1);
}

void test_get_servers_reference() {
    Config config;
    Server s;
    config.addServer(s);
    const std::vector<Server>& ref = config.getServers();
    assert(&ref == &config.getServers()); // must return reference, not copy
}

void test_move_constructor() {
    Config config;
    Server s;
    config.addServer(s);

    Config moved(std::move(config));
    assert(moved.getServers().size() == 1);
    assert(config.getServers().empty()); // moved-from should be valid, but empty
}

void test_move_assignment() {
    Config config;
    Server s;
    config.addServer(s);

    Config moved;
    moved = std::move(config);
    assert(moved.getServers().size() == 1);
    assert(config.getServers().empty());
}

int main() {
    test_default_is_empty();
    test_add_server();
    test_get_servers_reference();
    test_move_constructor();
    test_move_assignment();

    std::cout << "All Config tests passed.\n";
    return 0;
}
