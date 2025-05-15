/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   buildFilePath.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/14 16:12:07 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/15 17:07:17 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "utils/buildFilePath.hpp"
#include "utils/filesystemUtils.hpp"
#include <iostream>
#include <sys/stat.h>

static std::string joinPath(const std::string& base, const std::string& suffix) {
    if (base.empty())
        return suffix;
    if (base.back() == '/')
        return base + suffix;
    return base + '/' + suffix;
}

// Determine full file path based on request, and location
std::string buildFilePath(const HttpRequest& request, const Location& loc) {
    std::string request_path = request.getPath(); // /delete.html
    //std::cout << "REQUEST PATH: {" << request_path << "}" << std::endl;
    std::string location_root = loc.getRoot(); // /home/irychkov/Desktop/webserv_team/serverfiles/html
    //std::cout << "LOCATION ROOT: {" << location_root << "}" << std::endl;
    std::string location_path = loc.getPath();// /
    //std::cout << "LOCATION PATH: {" << location_path << "}" << std::endl;
    std::string index_file = loc.getIndex(); 

    std::string suffix;
    if (request_path.find(location_path) == 0)
        suffix = request_path.substr(location_path.length());
    if (!suffix.empty() && suffix[0] == '/')
        suffix.erase(0, 1);
    //std::cout << "SUFFIX: {" << suffix << "}" << std::endl;

    std::string full_path = joinPath(location_root, suffix);
    //std::cout << "full_path: {" << full_path << "}" << std::endl;
    bool endsWithSlash = !request_path.empty() && request_path.back() == '/';

    // Case: it's a directory path or the request ends with /
    struct stat s;
    if (stat(full_path.c_str(), &s) == 0 && S_ISDIR(s.st_mode)) {
        if (!index_file.empty()) {
            std::string index_path = joinPath(full_path, index_file);
            if (fileExists(index_path))
                return index_path;
        }
        return full_path; // will be handled as autoindex or directory
    }

    // Case: requested exact file, like /test/delete.html
    if (fileExists(full_path))
        return full_path;

    // Maybe it's requesting root path with trailing slash and we should serve index
    if ((endsWithSlash || suffix.empty()) && !index_file.empty()) {
        std::string index_path = joinPath(location_root, index_file);
        if (fileExists(index_path))
            return index_path;
    }
    std::cout << "full_path: {" << full_path << "}" << std::endl;

    return full_path;
}
