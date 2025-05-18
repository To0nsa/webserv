/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   buildFilePath.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/14 16:12:07 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/18 16:13:03 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "utils/buildFilePath.hpp"
#include "utils/filesystemUtils.hpp"

std::string joinPath(const std::string& base, const std::string& suffix) {
    if (base.empty())
        return suffix;
    if (base.back() == '/')
        return base + suffix;
    return base + '/' + suffix;
}

std::string buildFilePath(const HttpRequest& request, const Location& loc) {
    std::string request_path  = request.getPath();
    std::string location_path = loc.getPath();
    std::string location_root = loc.getRoot();

    std::string suffix;
    if (request_path.find(location_path) == 0)
        suffix = request_path.substr(location_path.length());

    if (!suffix.empty() && suffix[0] == '/')
        suffix.erase(0, 1);

    return joinPath(location_root, suffix); // May point to file or directory
}

static std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) parts.push_back(part);
    }
    return parts;
}

bool mkdirRecursive(const std::string& path) {
    std::vector<std::string> parts = splitPath(path);
    std::string current = path[0] == '/' ? "/" : "";

    for (size_t i = 0; i < parts.size(); ++i) {
        current = joinPath(current, parts[i]);
		if (fileExists(current)) {
            std::cerr << "[mkdirRecursive] Path exists and is a file (not directory): " << current << std::endl;
            return false;
        }
        if (mkdir(current.c_str(), 0777) == -1) {
            if (errno != EEXIST) {
                std::cerr << "[mkdirRecursive] Failed to create directory: " << current << " — errno: " << strerror(errno) << std::endl;
                return false;
            }
        }
    }
    return true;
}
