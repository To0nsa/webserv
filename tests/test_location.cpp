/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   test_location.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/09 09:43:13 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/09 09:43:14 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/Location.hpp"
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
    loc.addCgiExtension(".php");
    loc.addCgiExtension(".py");

    assert(loc.isUploadEnabled());
    assert(loc.isCgiRequest("/form.php"));
    assert(loc.isCgiRequest("/script.py"));
    assert(!loc.isCgiRequest("/form.txt"));
}

void test_index_resolution() {
    Location loc;
    loc.setRoot("/var/www");

    // No index file: should return empty string
    assert(loc.getEffectiveIndexPath().empty());

    // Add multiple index files
    loc.addIndexFile("index.html");
    loc.addIndexFile("index.htm");

    const std::vector<std::string>& indices = loc.getIndexFiles();
    assert(indices.size() == 2);
    assert(indices[0] == "index.html");
    assert(indices[1] == "index.htm");

    // Check effective path
    assert(loc.getEffectiveIndexPath() == "/var/www/index.html");
}

void test_redirect_and_autoindex() {
    Location loc;

    assert(!loc.hasRedirect());
    assert(!loc.isAutoindexEnabled());

    loc.setRedirect("/new-location", 301);
    assert(loc.hasRedirect());
    assert(loc.getRedirect() == "/new-location");
    assert(loc.getReturnCode() == 301);

    loc.setAutoindex(true);
    assert(loc.isAutoindexEnabled());

    loc.setAutoindex(false);
    assert(!loc.isAutoindexEnabled());
}

int main() {
    test_is_method_allowed();
    test_matches_path();
    test_resolve_absolute_path();
    test_upload_and_cgi_flags();
    test_index_resolution();
    test_redirect_and_autoindex();

    std::cout << "✅ All Location tests passed successfully.\n";
    return 0;
}
