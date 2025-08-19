/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Url.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/19 09:43:12 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/19 09:45:33 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Url.hpp
 * @brief   Declares the Url POD for parsed URL components.
 *
 * @details Lightweight structure holding parts of a parsed URL
 *          (scheme, userinfo, host, port, path, query, fragment).
 *          Used by the HTTP request parser to expose normalized URL
 *          fields to the router and handlers.
 *
 * @ingroup http
 */

#pragma once

#include <string>

/**
 * @brief Parsed URL components.
 *
 * @details Simple aggregate type (POD) storing the pieces of a URL.
 *          All fields are plain strings as parsed; no validation or
 *          normalization is performed here beyond what the parser supplies.
 *
 * @ingroup http
 */
struct Url {
    std::string scheme;   ///< URL scheme (e.g., "http", "https"); empty if absent.
    std::string user;     ///< User part of userinfo (before ':'); empty if absent.
    std::string password; ///< Password part of userinfo (after ':'); empty if absent.
    std::string host;     ///< Hostname or IP (without port).
    std::string port;     ///< Decimal port as string (e.g., "80"); empty if default/absent.
    std::string path;     ///< Path starting with '/', e.g. "/index.html"; may be empty.
    std::string query;    ///< Raw query string without leading '?'; may be empty.
    std::string fragment; ///< Fragment without leading '#'; may be empty.

    /** @name Construction & special members */
    ///@{
    Url()                      = default; ///< Default construct.
    Url(const Url&)            = default; ///< Copy construct.
    ~Url()                     = default; ///< Trivial destructor.
    Url& operator=(const Url&) = default; ///< Copy assign.
    ///@}
};
