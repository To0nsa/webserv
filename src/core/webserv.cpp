/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webserv.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/20 21:57:56 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/17 12:25:57 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/Config.hpp"              // for Config
#include "config/normalizeConfig.hpp"     // for normalizeConfig
#include "config/parser/ConfigParser.hpp" // for ConfigParser
#include "config/validateConfig.hpp"      // for validateConfig
#include "network/SocketManager.hpp"      // for SocketManager
#include "utils/printInfo.hpp"            // for printConfig, printUsage
#include <fstream>                        // for char_traits, basic_ifstream
#include <sstream>                        // for basic_ostringstream
#include <stdexcept>                      // for runtime_error
#include <stdlib.h>                       // for EXIT_SUCCESS
#include <string>                         // for string, allocator, operator+
#include <string_view>                    // for string_view

namespace {
inline constexpr std::string_view DEFAULT_CONFIG_PATH{"./configs/default.conf"};

std::string resolveConfigPath(int argc, char** argv) {
    std::string config_path;
    if (argc == 1) {
        config_path = DEFAULT_CONFIG_PATH;
    } else if (argc == 2) {
        config_path = argv[1];
    } else if (argc > 2) {
        throw std::runtime_error(printUsage());
    }
    return config_path;
}

std::string extractFileContent(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file) {
        throw std::runtime_error("Failed to open config file: " + config_path);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

Config loadConfig(const std::string& fileContent) {
    ConfigParser parser(fileContent);
    return parser.parseConfig();
}
} // namespace

int runWebserv(int argc, char** argv) {
    std::string configPath  = resolveConfigPath(argc, argv);
    std::string fileContent = extractFileContent(configPath);
    Config      config      = loadConfig(fileContent);
    normalizeConfig(config);
    validateConfig(config);
    printConfig(config);
    SocketManager manager(config.getServers());
    manager.run();
    return EXIT_SUCCESS;
}