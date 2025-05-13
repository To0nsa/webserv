/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 13:11:30 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/12 20:10:55 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/Config.hpp"
#include "config/normalizer/ConfigNormalizer.hpp"
#include "config/parser/ConfigParser.hpp"
#include "config/validator/ConfigValidator.hpp"
#include "network/SocketManager.hpp"
#include "utils/PrintInfo.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

int main(int ac, char** av) {
    std::string config_file = (ac == 2) ? av[1] : "./configs/default.conf";
    if (ac > 2) {
        print_usage();
        return 1;
    }

    try {
        // Load file content into string
        std::ifstream input(config_file);
        if (!input) {
            std::cerr << "Failed to open config file: " << config_file << std::endl;
            return 1;
        }
        std::stringstream buffer;
        buffer << input.rdbuf();

        // Parse config
        ConfigParser parser(buffer.str());
        Config       config = parser.parseConfig();

        // Normalize + Validate
        for (Server& s : config.getServers())
            normalizeServer(s);
        ConfigValidator::validate(config);

        // Optional: debug print of the parsed config
        print_config(config);

        // Launch server
        SocketManager manager(config.getServers());
        manager.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
