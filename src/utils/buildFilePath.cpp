/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   buildFilePath.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/14 16:12:07 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/15 10:47:30 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "utils/buildFilePath.hpp"
#include <iostream>

static std::string joinPath(const std::string& root, const std::string& suffix) {
	std::string result = root;
	if (root.empty() || root[root.length() - 1] != '/')
		result += "/";
	result += suffix;
	return result;
}

// Determine full file path based on request, and location
std::string buildFilePath(const HttpRequest& request, const Location& loc) {
	std::string request_path = request.getPath(); // /delete.html
	//std::cout << "REQUEST PATH: {" << request_path << "}" << std::endl;
	std::string location_root = loc.getRoot(); // /home/irychkov/Desktop/webserv_team/serverfiles/html
	//std::cout << "LOCATION ROOT: {" << location_root << "}" << std::endl;
	std::string location_path = loc.getPath();// /
	//std::cout << "LOCATION PATH: {" << location_path << "}" << std::endl;
	std::string fullPath;

	std::string suffix = request_path.substr(location_path.length());
	//std::cout << "SUFFIX: {" << suffix << "}" << std::endl;
	if (!suffix.empty() && suffix[0] == '/')
		suffix = suffix.substr(1);
	//std::cout << "SUFFIX: {" << suffix << "}" << std::endl;

	fullPath = joinPath(location_root, suffix);
	//std::cout << "fullPath: {" << fullPath << "}" << std::endl;

	bool endsWithSlash = false;
	if (!request_path.empty()) {
		char last = request_path[request_path.length() - 1];
		if (last == '/') {
			endsWithSlash = true;
		}
	}
	bool suffixEmpty = suffix.empty();

	if ((fullPath == location_root + "/") && !loc.getIndex().empty()) {
		if (endsWithSlash || suffixEmpty) {
			fullPath = joinPath(location_root, loc.getIndex());
		}
	}
	//std::cout << "fullPath: {" << fullPath << "}" << std::endl;

	return fullPath;
}
