/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:31:10 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/28 23:06:39 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "http/Url.hpp"
#include "utils/stringUtils.hpp"
#include <map>
#include <string>

class HttpRequest {

  private:
    std::string                        _method;
    std::string                        _path;
    std::string                        _version;
    std::map<std::string, std::string> _headers;
    std::string                        _body;
    std::size_t                        _contentLength{0};
    std::string /* _uri; */            _query; ///< extracted from URI after '?'
    Url                                _url;

  public:
    HttpRequest(void);
    ~HttpRequest(void);

    void printRequest(void) const;

    const std::string&                        getMethod(void) const;
    const std::string&                        getPath(void) const;
    const std::string&                        getVersion(void) const;
    const std::string&                        getHeader(const std::string& key) const;
    const std::map<std::string, std::string>& getHeaders() const;
    const std::string&                        getBody(void) const;
    std::size_t                               getContentLength(void) const;
    /* const std::string& getUri(void) const; */ const std::string& getQuery() const;

    void setMethod(const std::string& method);
    void setPath(const std::string& path);
    void setVersion(const std::string& version);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    void setContentLength(size_t len);
    void setUrl(const Url& url);
    void setQuery(const std::string& query);

    bool hasHeader(const std::string& key) const;
};
