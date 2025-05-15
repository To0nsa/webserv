/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   buildFilePath.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/14 16:12:07 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/15 19:18:49 by irychkov         ###   ########.fr       */
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
