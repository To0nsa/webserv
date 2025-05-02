/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FilePath.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/01 12:42:38 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/01 13:49:05 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "FilePath.hpp"
#include <vector>
#include <string>
#include <algorithm>

// Helper: Check if request path starts with location path
static bool pathMatches(const std::string& requestPath, const std::string& locationPath) {
	if (locationPath == "/")
		return true;
	return requestPath.compare(0, locationPath.length(), locationPath) == 0;
}

// Join root and suffix safely
static std::string joinPath(const std::string& root, const std::string& suffix) {
	std::string result = root;
	if (root.empty() || root[root.length() - 1] != '/')
		result += "/";
	result += suffix;
	return result;
}

// Main function: determine full file path based on server config and request
std::string buildFilePath(const Server& server, const HttpRequest& request) {
	const std::vector<Location>& locations = server.getLocations();
	std::string uri = request.getPath();
	std::string fullPath;
	bool matched = false;

	for (size_t i = 0; i < locations.size(); ++i) {
		const Location& loc = locations[i];
		if (pathMatches(uri, loc.path)) {
			std::string suffix = uri.substr(loc.path.length());
			if (!suffix.empty() && suffix[0] == '/')
				suffix = suffix.substr(1);

			fullPath = joinPath(loc.root, suffix);

			bool endsWithSlash = false;
			if (!uri.empty()) {
				char last = uri[uri.length() - 1];
				if (last == '/') {
					endsWithSlash = true;
				}
			}
			bool suffixEmpty = suffix.empty();

			if (!loc.index.empty()) {
				if (endsWithSlash || suffixEmpty) {
					fullPath = joinPath(loc.root, loc.index);
				}
			}
			matched = true;
			break;
		}
	}

	// If no match, use default
	if (!matched) {
		std::string root = ".";
		fullPath = root + uri;
	}

	return fullPath;
}
