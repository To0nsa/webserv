/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:31:10 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/11 12:31:20 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <map>
#include <string>

class HttpRequest {

  private:
    std::string                        _method;
    std::string                        _path;
    std::string                        _version;
    std::map<std::string, std::string> _headers;
    std::string                        _body;
    std::size_t                        _contentLength { 0 };

  public:
    HttpRequest(void);
    ~HttpRequest(void);

    bool parse(const std::string& raw_request);
    void printRequest(void) const;

    const std::string& getMethod(void) const;
    const std::string& getPath(void) const;
    const std::string& getVersion(void) const;
    const std::string& getHeader(const std::string& key) const;
    const std::string& getBody(void) const;

    void setMethod(const std::string& method);
    void setPath(const std::string& path);
    void setVersion(const std::string& version);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    void setContentLength(size_t len);
};
