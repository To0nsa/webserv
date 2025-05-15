/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_get.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/15 12:39:41 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/15 12:59:12 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handle_get.hpp"

static std::string truncateName(const std::string& name, std::size_t maxLen) {
	if (name.length() <= maxLen)
		return name;
	if (maxLen <= 2)
		return std::string(maxLen, '.'); // fallback: "..", ".", or ""
	return name.substr(0, maxLen - 2) + "..";
}

HttpResponse handleGet(const HttpRequest &request, const Server &server, const Location &loc) {
	std::string filepath = buildFilePath(request, loc);
	std::cout << "Resolved file path: " << filepath << std::endl;

	// Check if path is a directory
	bool isDirectory = false;
	struct stat fileStat;
	if (stat(filepath.c_str(), &fileStat) == 0 && S_ISDIR(fileStat.st_mode)) {
		std::cout << "Path is a directory.\n";
		isDirectory = true;
	}
	std::string body;
	if (isDirectory) {
		if (loc.isAutoindexEnabled()) {
			DIR* dir = opendir(filepath.c_str());
			if (dir == NULL) {
				std::cout << "[AUTOINDEX] Failed to open directory!" << std::endl;
			}
			if (dir) {
				struct dirent* entry;
				std::stringstream indexStream;
				std::string baseUri = request.getPath();
				if (baseUri.empty() || baseUri.back() != '/')
					baseUri += '/';
				indexStream << "<html><body><h1> &nbsp;&nbsp; Index of " << request.getPath() << "</h1><table><tr><th>Name</th><th>Last Modified (UTC)</th><th>Size (bytes)</th></tr>";
				while ((entry = readdir(dir)) != NULL) {
				std::string name = entry->d_name;
				if (name == ".")
					continue;
				if (name == "..") {
					name += "/";
					indexStream << "<tr><td style='min-width: 20ch; padding-right: 30px;'><a href='" << baseUri << name << "'>" << name << "</a></td></tr>";
					continue;
				}

				std::string fullPath = filepath + "/" + name;
				struct stat fileStat;
				if (stat(fullPath.c_str(), &fileStat) == -1) {
					perror(("stat failed for " + fullPath).c_str());
					continue;
				}

				std::stringstream sizeStream;
				sizeStream << fileStat.st_size << " bytes";

				auto mod_time = std::chrono::system_clock::from_time_t(fileStat.st_mtime);
				std::time_t time = std::chrono::system_clock::to_time_t(mod_time);
				std::stringstream timeStream;
				timeStream << std::put_time(std::gmtime(&time), "%d-%b-%Y %H:%M");

				std::string displayName = truncateName(name, 25);
				if (S_ISDIR(fileStat.st_mode)) {
					displayName += "/";
				}

				indexStream << "<tr>";
				indexStream << "<td style='min-width: 20ch; padding-right: 30px;'><a href='" << baseUri << name << "'>" << displayName << "</a></td>";
				indexStream << "<td style='min-width: 20ch; padding-right: 30px;'>" << timeStream.str() << "</td>";
				// Only show size for regular files
				if (S_ISREG(fileStat.st_mode)) {
					indexStream << "<td style='min-width: 20ch; padding-right: 30px;'>" << fileStat.st_size << "</td>";
				} else {
					indexStream << "<td style='min-width: 20ch; padding-right: 30px;'>-</td>";
				}
				indexStream << "</tr>";
			}
				indexStream << "</table></body></html>";
				closedir(dir);
				body = indexStream.str();
			}
		}
	} else {
		return serveFile(filepath, request, "");
		
	}

	if (body.empty()) {
		return ResponseBuilder::generateError(404, server, request);
	}
	return ResponseBuilder::generateSuccess(200, body, "text/html", request);
}
