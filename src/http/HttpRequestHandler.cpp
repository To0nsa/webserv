/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestHandler.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/12 23:13:23 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/15 10:54:51 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpRequestHandler.hpp"
#include "core/Location.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/buildFilePath.hpp"
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <sstream>

HttpResponse handleGet(const HttpRequest&, const Server&, const Location&);
HttpResponse handlePost(const HttpRequest&, const Server&, const Location&);
HttpResponse handleDelete(const HttpRequest&, const Server&, const Location&);
HttpResponse handleCgi(const HttpRequest&, const Server&, const Location&);

HttpResponse handleRequest(const HttpRequest& request, const Server& server) {
    const std::string& method = request.getMethod();
    const std::string& path   = request.getPath();

    // Find matching location
    const Location* matched = nullptr;
    for (const Location& loc : server.getLocations()) {
        if (loc.matchesPath(path)) {
            matched = &loc;
            break;
        }
    }

    if (!matched) {
        return ResponseBuilder::generateError(404, server, request);
    }

    const Location& location = *matched;

    // Handle HTTP redirection
    if (location.hasRedirect()) {
        return ResponseBuilder::generateRedirect(location.getReturnCode(), location.getRedirect(),
                                                 request);
    }

    // Method not allowed
    if (!location.isMethodAllowed(method)) {
        return ResponseBuilder::generateError(405, server, request);
    }

    // CGI detection
    if (location.isCgiRequest(path)) {
        return handleCgi(request, server, location);
    }

    // Delegate based on method
    if (method == "GET") {
        return handleGet(request, server, location);
    } else if (method == "POST") {
        return handlePost(request, server, location);
    } else if (method == "DELETE") {
        return handleDelete(request, server, location);
    }

    return ResponseBuilder::generateError(501, server, request); // Not implemented
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
				indexStream << "<html><body><h1>Index of " << request.getPath() << "</h1><ul>";
				while ((entry = readdir(dir)) != NULL) {
					std::string name = entry->d_name;
					if (name == ".")
						continue;
					if (name == "..")
						name += "/";
					indexStream << "<li style=\"list-style: none;\"><a href='" << baseUri << name << "'>" << name << "</a></li>"; // Think about .. and .
				}
				indexStream << "</ul></body></html>";
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

HttpResponse handlePost(const HttpRequest &request, const Server &server, const Location &loc) {
	(void)loc;
	(void)server;
	return ResponseBuilder::generateSuccess(200, "<h1>Success</h1><p>OK</p>", "text/html", request);
}

HttpResponse handleDelete(const HttpRequest &request, const Server &server, const Location &loc) {
	// Build full file path
	const std::string& request_path = request.getPath();
	std::string suffix = request_path.substr(loc.getPath().length());
	if (!suffix.empty() && suffix[0] == '/')
		suffix = suffix.substr(1);
	std::string filepath = loc.getRoot();
	if (!filepath.empty() && filepath[filepath.size() - 1] != '/')
		filepath += "/";
	filepath += suffix;

	// Check if file exists and delete
	struct stat s;
	if (stat(filepath.c_str(), &s) != 0)
		return ResponseBuilder::generateError(404, server, request);
	if (!S_ISREG(s.st_mode))
		return ResponseBuilder::generateError(403, server, request);
	if (unlink(filepath.c_str()) != 0)
		return ResponseBuilder::generateError(500, server, request);
	return ResponseBuilder::generateSuccess(200, "<h1>File " + suffix + " deleted.</h1>", "text/html", request);
}

HttpResponse handleCgi(const HttpRequest &request, const Server &server, const Location &loc) {
	(void)loc;
	(void)server;
	return ResponseBuilder::generateSuccess(200, "<h1>Success</h1><p>OK</p>", "text/html", request);
}

