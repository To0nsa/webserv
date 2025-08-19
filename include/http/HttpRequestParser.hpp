/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestParser.hpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/19 09:33:41 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/19 09:35:32 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    HttpRequestParser.hpp
 * @brief   Declares the HttpRequestParser utility class.
 *
 * @details Provides a static interface for parsing raw HTTP request strings
 *          into structured @ref HttpRequest objects. The parser extracts and
 *          validates the request line, headers, and body while handling
 *          connection-specific details such as consumed byte counts.
 *
 *          The main entry point is @ref HttpRequestParser::parse, which:
 *          - Populates an @ref HttpRequest with parsed fields.
 *          - Selects the matching @ref Server configuration from the list
 *            of available virtual hosts on the same port.
 *          - Returns any parsing error code and the number of bytes consumed
 *            from the raw input buffer.
 *
 *          The class is non-instantiable and only exposes static methods.
 *
 * @ingroup http
 */

#pragma once

#include <cstddef> // for size_t
#include <string>  // for string
#include <vector>  // for vector
class HttpRequest;
class Server;

/**
 * @class   HttpRequestParser
 * @brief   Utility class for parsing raw HTTP requests.
 *
 * @details Provides a static method @ref parse that converts a raw HTTP
 *          request string into a structured @ref HttpRequest object.
 *          This includes:
 *          - Extracting the request line (method, path, version).
 *          - Parsing headers into a normalized map.
 *          - Capturing the body (if any).
 *          - Identifying the matching @ref Server configuration
 *            from a list of virtual hosts bound to the same port.
 *          - Returning parsing errors and the number of bytes consumed
 *            from the raw input buffer.
 *
 *          The class is non-instantiable and non-copyable: it only
 *          exposes static parsing functionality and deletes its
 *          constructors and operators.
 *
 * @ingroup http
 */
class HttpRequestParser {
  public:
    //=== Parsing API ========================================================
    static bool parse(HttpRequest& req, const std::string& raw_req,
                      std::vector<Server> serversOnPort, int& errorCode,
                      std::size_t& consumedBytes);

  private:
    //=== Non-instantiable utility class =====================================

    HttpRequestParser()  = delete; ///< Deleted: non-instantiable.
    ~HttpRequestParser() = delete; ///< Deleted: prevents accidental instantiation.
    HttpRequestParser(const HttpRequestParser& org) = delete; ///< Deleted: non-copyable.
    HttpRequestParser&
    operator=(const HttpRequestParser& other) = delete; ///< Deleted: non-assignable.
};
