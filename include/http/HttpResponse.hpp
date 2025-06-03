/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 10:55:37 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/03 13:46:46 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

#include <map>
#include <string>
#include <unistd.h>

class HttpResponse {
  private:
    int                                _status_code;
    std::string                        _status_message;
    std::map<std::string, std::string> _headers;
    std::string                        _body;
    std::string                        _http_version;
    std::string                        _connection_header;
	std::string                        _file_path;
	std::string                        _cgi_temp_file;
	std::streamsize _cgiBodyOffset;

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
	void        setFilePath(const std::string& path);
	const std::string& getFilePath() const;
	bool isFileResponse() const;
	int         getStatusCode(void) const;
	const std::string& getStatusMessage(void) const;
	const std::map<std::string, std::string>& getHeaders(void) const;
	void setCgiBodyOffset(std::streamsize offset);
	std::streamsize getCgiBodyOffset() const;
	void setCgiTempFile(const std::string& temp_file);
	const std::string& getCgiTempFile() const;
	bool isCgiTempFile() const;
};
