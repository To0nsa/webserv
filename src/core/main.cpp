/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:11:30 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/10 00:07:50 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    main.cpp
 * @brief   Entry point for the Webserv application.
 *
 * @details Initializes and runs the Webserv HTTP server.
 *          Delegates execution to `runWebserv`, handling all uncaught exceptions.
 * @ingroup core
 */

#include "core/webserv.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

/**
 * @brief Program entry point.
 *
 * @details Starts the Webserv server using the provided configuration file path
 *          or a default configuration if none is given.
 *
 * @param argc Argument count.
 * @param argv Argument vector. The first optional argument is the path to the configuration file.
 * @return `EXIT_SUCCESS` on successful shutdown, `EXIT_FAILURE` on error.
 *
 * @throws std::runtime_error If an unrecoverable error occurs during startup.
 */
int main(int argc, char** argv) try {
    return runWebserv(argc, argv);
} catch (const std::exception& e) {
    std::cerr << "webserv error: " << e.what() << std::endl;
    return EXIT_FAILURE;
} catch (...) {
    std::cerr << "webserv encountered an unexpected error" << std::endl;
    return EXIT_FAILURE;
}
