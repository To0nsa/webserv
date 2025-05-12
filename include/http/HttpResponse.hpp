/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 10:55:37 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/12 23:09:20 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

#include <map>
#include <string>

class HttpResponse {
  private:
    int                                _status_code;
    std::string                        _status_message;
    std::map<std::string, std::string> _headers;
    std::string                        _body;
    std::string                        _http_version;
    std::string                        _connection_header;

  public:
    HttpResponse(void);
    ~HttpResponse(void);
    HttpResponse(const HttpResponse& other)            = default;
    HttpResponse& operator=(const HttpResponse& other) = default;

    void        setStatus(int code, const std::string& message);
    void        setHeader(const std::string& key, const std::string& value);
    void        setBody(const std::string& body);
    void        setRequestMeta(const std::string& version, const std::string& conn);
    bool        isConnectionClose(void) const;
    std::string toHttpString(void) const;
};
