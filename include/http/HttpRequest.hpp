/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 12:31:10 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/15 00:35:40 by nlouis           ###   ########.fr       */
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
    std::string                        _query; ///< extracted from URI after '?'

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
    const std::string& getQuery() const;
};
