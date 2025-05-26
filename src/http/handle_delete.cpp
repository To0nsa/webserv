/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handle_delete.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/21 15:06:07 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/26 16:48:41 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/handle_delete.hpp"

HttpResponse handleDelete(const HttpRequest& request, const Server& server, const Location& loc) {
    // Build full file path
    std::string filepath = buildFilePath(request, loc);
	std::cout << "Resolved file path: " << filepath << std::endl;

    // Check if file exists and delete
    struct stat s;
    if (stat(filepath.c_str(), &s) != 0)
        return ResponseBuilder::generateError(404, server, request);
    if (!S_ISREG(s.st_mode))
        return ResponseBuilder::generateError(403, server, request);
    if (unlink(filepath.c_str()) != 0)
        return ResponseBuilder::generateError(500, server, request);
    std::string filename = request.getPath().substr(request.getPath().find_last_of("/") + 1);
    return ResponseBuilder::generateSuccess(200, "<h1>File " + filename + " deleted.</h1>",
                                            "text/html", request);
}
