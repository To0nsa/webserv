/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleCgi.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/15 00:24:43 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/03 13:25:03 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "core/Location.hpp"
#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

#include <optional>
#include <poll.h>
#include <string>
#include <unistd.h>
#include <vector>

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

bool         initCgiProcess(CgiProcess& cgi, const HttpRequest& request, const Server& server,
                            const Location& loc, const std::vector<pollfd>& poll_fds);
HttpResponse finalizeCgi(CgiProcess& cgi, const Server& server, const HttpRequest& request);
void         cleanupCgi(CgiProcess& cgi);
void         errorOnCgi(CgiProcess& cgi);
bool         tryTerminateCgi(CgiProcess& cgi);

} // namespace CGI
