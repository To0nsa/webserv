/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:11:30 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/24 23:18:25 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/webserv.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) try {
    return runWebserv(argc, argv);
} catch (const std::exception& e) {
    std::cerr << "webserv error: " << e.what() << std::endl;
    return EXIT_FAILURE;
} catch (...) {
    std::cerr << "webserv encountered an unexpected error" << std::endl;
    return EXIT_FAILURE;
}