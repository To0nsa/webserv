/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HandlePostUpload.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/01 18:30:47 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/01 18:31:30 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HandlePostUpload.hpp"
#include <fstream>
#include <sstream>
#include <ctime>
#include <sys/stat.h>

std::string handlePostUpload(const Server& server, const HttpRequest& request) {
	// Reject if body is too large
	if (request.getBody().size() > server.getClientMaxBodySize()) {
		return "ERROR:413";
	}

	const std::vector<Location>& locations = server.getLocations();
	std::string uri = request.getPath();

	for (size_t i = 0; i < locations.size(); ++i) {
		const Location& loc = locations[i];

		// Check if the request matches this location and POST is allowed
		if (uri.find(loc.path) == 0) {
			bool methodAllowed = false;
			for (size_t m = 0; m < loc.methods.size(); ++m) {
				if (loc.methods[m] == "POST") {
					methodAllowed = true;
					break;
				}
			}
			if (!methodAllowed)
				return "ERROR:405";
			if (loc.upload_store.empty())
				return "ERROR:403";

			// Create a unique filename using timestamp
			time_t now = time(0);
			std::stringstream filename;
			filename << loc.upload_store;
			if (!loc.upload_store.empty() && loc.upload_store[loc.upload_store.size() - 1] != '/')
				filename << "/";
			filename << "upload_" << now << ".txt";

			// Write the body to the file
			std::ofstream out(filename.str().c_str());
			if (!out.is_open())
				return "ERROR:500";
			out << request.getBody();
			out.close();

			return filename.str(); // Success
		}
	}

	return "ERROR:404"; // No matching upload route
}
