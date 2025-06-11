/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:11:30 by irychkov          #+#    #+#             */
/*   Updated: 2025/06/11 09:23:42 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/webserv.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

/* int main(int argc, char** argv) try {
    return runWebserv(argc, argv);
} catch (const std::exception& e) {
    std::cerr << "webserv error: " << e.what() << std::endl;
    return EXIT_FAILURE;
} catch (...) {
    std::cerr << "webserv encountered an unexpected error" << std::endl;
    return EXIT_FAILURE;
} */

#include "core/webserv.hpp"
#include "config/parser/ConfigParseError.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) try {
    return runWebserv(argc, argv);
} catch (const ConfigParseError& e) {
    std::cerr << "Configuration parse error: " << e.what() << std::endl;
    return EXIT_FAILURE;
} catch (const std::exception& e) {
    std::cerr << "webserv error: " << e.what() << std::endl;
    return EXIT_FAILURE;
} catch (...) {
    std::cerr << "webserv encountered an unexpected error" << std::endl;
    return EXIT_FAILURE;
}