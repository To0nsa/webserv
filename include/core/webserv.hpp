/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/20 22:47:01 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/13 22:36:29 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    webserv.hpp
 * @brief   Public entry point for starting the Webserv server.
 *
 * @details Declares the `runWebserv()` function, which bootstraps the
 *          configuration loading, validation, and event loop execution.
 * @ingroup core
 */

#pragma once

int runWebserv(int argc, char** argv); // documented at ./src/core/webserv.cpp