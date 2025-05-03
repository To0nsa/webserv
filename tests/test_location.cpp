/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   test_location.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/02 17:24:13 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/02 17:31:16 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/Location.hpp"
#include <cassert>
#include <iostream>

void test_is_method_allowed() {
    Location loc;
    loc.addMethod("GET");
    loc.addMethod("POST");

    assert(loc.isMethodAllowed("GET"));
    assert(loc.isMethodAllowed("POST"));
    assert(!loc.isMethodAllowed("DELETE"));
}

void test_matches_path() {
    Location loc;
    loc.setPath("/api");

    assert(loc.matchesPath("/api"));
    assert(loc.matchesPath("/api/users"));
    assert(!loc.matchesPath("/about"));
}

void test_resolve_absolute_path() {
    Location loc;
    loc.setPath("/static");
    loc.setRoot("/var/www");

    assert(loc.resolveAbsolutePath("/static/logo.png") == "/var/www/logo.png");
    assert(loc.resolveAbsolutePath("/static") == "/var/www");
    assert(loc.resolveAbsolutePath("/unmatched") == "");
}

void test_upload_and_cgi_flags() {
    Location loc;
    assert(!loc.isUploadEnabled());
    assert(!loc.isCgiRequest("/index.php"));

    loc.setUploadStore("/uploads");
    loc.setCgiExtension(".php");

    assert(loc.isUploadEnabled());
    assert(loc.isCgiRequest("/form.php"));
    assert(!loc.isCgiRequest("/form.py"));
}

void test_index_resolution() {
    Location loc;
    loc.setRoot("/var/www");
    assert(loc.getEffectiveIndexPath().empty());

    loc.setIndex("index.html");
    assert(loc.getEffectiveIndexPath() == "/var/www/index.html");
}

int main() {
    test_is_method_allowed();
    test_matches_path();
    test_resolve_absolute_path();
    test_upload_and_cgi_flags();
    test_index_resolution();

    std::cout << "✅ All Location tests passed successfully.\n";
    return 0;
}
