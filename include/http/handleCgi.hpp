/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleCgi.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/15 00:24:43 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/24 23:12:18 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "core/Location.hpp"
#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

#include <optional>
#include <string>
#include <unistd.h>

struct CgiProcess {
    enum class Phase { Launching, Writing, Reading, Done, Failed };

    pid_t       pid       = -1;
    int         stdin_fd  = -1;
    int         stdout_fd = -1;
    std::string input;  ///< Body to send to CGI (for POST)
    std::string output; ///< Output received from CGI
    std::size_t input_sent    = 0;
    Phase       phase         = Phase::Launching;
    time_t      start_time    = 0;
    time_t      last_activity = 0;
    std::string script_path; ///< Path to the CGI script
    std::string interpreter; ///< Interpreter to use
};

namespace CGI {

bool initCgiProcess(CgiProcess& cgi, const HttpRequest& request, const Server& server,
                    const Location& loc);
bool handleWrite(CgiProcess& cgi);
bool handleRead(CgiProcess& cgi);
std::optional<HttpResponse> finalizeCgi(CgiProcess& cgi, const Server& server,
                                        const HttpRequest& request);
void                        cleanupCgi(CgiProcess& cgi);
bool                        tryTerminateCgi(CgiProcess& cgi);

} // namespace CGI
