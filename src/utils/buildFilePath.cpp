/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   buildFilePath.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/14 16:12:07 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/14 18:06:27 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "utils/buildFilePath.hpp"

static std::string joinPath(const std::string& root, const std::string& suffix) {
	std::string result = root;
	if (root.empty() || root[root.length() - 1] != '/')
		result += "/";
	result += suffix;
	return result;
}

// Determine full file path based on server config, request, and location
std::string buildFilePath(const HttpRequest& request, const Location& loc) {
	std::string request_path = request.getPath();
	std::string fullPath;

	std::string suffix = request_path.substr(loc.getPath().length());
	if (!suffix.empty() && suffix[0] == '/')
		suffix = suffix.substr(1);

	fullPath = joinPath(loc.getRoot(), suffix);

	bool endsWithSlash = false;
	if (!request_path.empty()) {
		char last = request_path[request_path.length() - 1];
		if (last == '/') {
			endsWithSlash = true;
		}
	}
	bool suffixEmpty = suffix.empty();

	if (!loc.getIndex().empty()) {
		if (endsWithSlash || suffixEmpty) {
			fullPath = joinPath(loc.getRoot(), loc.getIndex());
		}
	}

	return fullPath;
}
