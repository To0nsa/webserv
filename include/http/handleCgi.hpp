/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleCgi.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/15 00:24:43 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/06 13:22:10 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "http/HttpResponse.hpp" // for HttpResponse
#include <string>                // for string
#include <time.h>                // for time_t
#include <unistd.h>              // for pid_t
#include <vector>                // for vector

class HttpRequest;
class Location;
class Server;
struct pollfd;

struct CgiProcess {
    pid_t       pid           = -1;
    time_t      start_time    = 0;
    time_t      last_activity = 0;
    std::string script_path; ///< Path to the CGI script
    std::string interpreter; ///< Interpreter to use
    std::string output_path; ///< Path to the temporary output file
    std::string input_path;  ///< Path to the temporary input file
};

namespace CGI {

void         unlinkWithErrorLog(const std::string& path, const std::string& context);
bool         initCgiProcess(CgiProcess& cgi, const HttpRequest& request, const Server& server,
                            const Location& loc, const std::vector<pollfd>& poll_fds, int& errorCode);
HttpResponse finalizeCgi(CgiProcess& cgi, const Server& server, const HttpRequest& request);
void         cleanupCgi(CgiProcess& cgi);
void         errorOnCgi(CgiProcess& cgi);
bool         tryTerminateCgi(CgiProcess& cgi);

} // namespace CGI
