/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_get.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/15 12:39:41 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/15 17:26:55 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handle_get.hpp"
#include <string_view>

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

	// Redirect if directory is missing trailing slash
	struct stat fileStat;
	if (stat(filepath.c_str(), &fileStat) == 0 && S_ISDIR(fileStat.st_mode)) {
		const std::string& path = request.getPath();
		if (path.empty() || path.back() != '/') {
			std::string location = request.getPath() + "/";
			return ResponseBuilder::generateRedirect(301, location, request);
		}
	}

	bool isDirectory = stat(filepath.c_str(), &fileStat) == 0 && S_ISDIR(fileStat.st_mode);
	std::string body;
	if (isDirectory) {
		if (loc.isAutoindexEnabled()) {
			DIR* dir = opendir(filepath.c_str());
			if (dir == NULL) {
				return ResponseBuilder::generateError(403, server, request);
			}
			if (dir) {
				struct dirent* entry;
				std::vector<std::string> dirs;
				std::vector<std::string> files;

				// Split entries into folders and files
				while ((entry = readdir(dir)) != NULL) {
					std::string name = entry->d_name;
					if (name == ".")
						continue;
					if (name == "..") {
						dirs.insert(dirs.begin(), name); // Ensure ".." is always first
						continue;
					}

					std::string fullPath = filepath + "/" + name;
					struct stat fileStat;
					if (stat(fullPath.c_str(), &fileStat) == -1) {
						perror(("stat failed for " + fullPath).c_str());
						continue;
					}

					if (S_ISDIR(fileStat.st_mode))
						dirs.push_back(name);
					else
						files.push_back(name);
				}
				closedir(dir);

				// Sort alphabetically
				if (!dirs.empty() && dirs[0] == "..")
					std::sort(dirs.begin() + 1, dirs.end()); // Skip ".."
				else
					std::sort(dirs.begin(), dirs.end());
				std::sort(files.begin(), files.end());

				// Start building the HTML
				std::stringstream indexStream;
				std::string baseUri = request.getPath();
				if (baseUri.empty() || baseUri.back() != '/')
					baseUri += '/';

				indexStream << "<html><body><h1> &nbsp;&nbsp; Index of " << request.getPath() << "</h1>";
				indexStream << "<table><tr><th>Name</th><th>Last Modified (UTC)</th><th>Size (bytes)</th></tr>";

				// Helper lambda to render a row
				auto renderEntry = [&](const std::string& name) {
					std::string fullPath = filepath + "/" + name;
					struct stat fileStat;
					if (stat(fullPath.c_str(), &fileStat) == -1)
						return;

					std::stringstream timeStream;
					auto mod_time = std::chrono::system_clock::from_time_t(fileStat.st_mtime);
					std::time_t time = std::chrono::system_clock::to_time_t(mod_time);
					timeStream << std::put_time(std::gmtime(&time), "%d-%b-%Y %H:%M");

					std::string displayName = truncateName(name, 25);
					std::string href = baseUri + name;

					bool isDir = S_ISDIR(fileStat.st_mode);
					if (isDir) {
						displayName += "/";
						href += "/";
					}

					indexStream << "<tr>";
					indexStream << "<td style='min-width: 20ch; padding-right: 30px;'><a href='" << href << "'>" << displayName << "</a></td>";
					indexStream << "<td style='min-width: 20ch; padding-right: 30px;'>" << timeStream.str() << "</td>";
					indexStream << "<td style='min-width: 20ch; padding-right: 30px;'>";
					if (isDir)
						indexStream << "-";
					else
						indexStream << fileStat.st_size;
					indexStream << "</td></tr>";
				};

				// Render ".." if present
				if (!dirs.empty() && dirs[0] == "..") {
					indexStream << "<tr><td colspan='3'><a href='" << baseUri << "../'>../</a></td></tr>";
					dirs.erase(dirs.begin());
				}

				// Render sorted directories
				for (const std::string& dirName : dirs)
					renderEntry(dirName);

				// Render sorted files
				for (const std::string& fileName : files)
					renderEntry(fileName);

				indexStream << "</table></body></html>";
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
