/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HandleDelete.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/01 19:01:54 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/01 19:02:11 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HandleDelete.hpp"
#include "Location.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <string>

// Utility to match request URI with a location path
static bool pathMatches(const std::string& uri, const std::string& location) {
	if (location == "/")
		return true;
	return uri.compare(0, location.size(), location) == 0;
}

std::string handleDelete(const Server& server, const HttpRequest& request) {
	const std::string& uri = request.getPath();
	const std::vector<Location>& locations = server.getLocations();

	for (size_t i = 0; i < locations.size(); ++i) {
		const Location& loc = locations[i];
		if (pathMatches(uri, loc.path)) {
			// Check if DELETE is allowed
			bool allowed = false;
			for (size_t m = 0; m < loc.methods.size(); ++m) {
				if (loc.methods[m] == "DELETE") {
					allowed = true;
					break;
				}
			}
			if (!allowed)
				return "405";

			// Build full file path
			std::string suffix = uri.substr(loc.path.length());
			if (!suffix.empty() && suffix[0] == '/')
				suffix = suffix.substr(1);
			std::string filepath = loc.root;
			if (!filepath.empty() && filepath[filepath.size() - 1] != '/')
				filepath += "/";
			filepath += suffix;

			// Check if file exists and delete
			struct stat s;
			if (stat(filepath.c_str(), &s) != 0)
				return "404";
			if (!S_ISREG(s.st_mode))
				return "403";
			if (unlink(filepath.c_str()) != 0)
				return "500";

			return "200";
		}
	}

	return "404"; // No matching location
}
