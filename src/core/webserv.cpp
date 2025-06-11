/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webserv.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/20 21:57:56 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/11 09:22:15 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/Config.hpp"
#include "config/normalizeConfig.hpp"
#include "config/parser/ConfigParser.hpp"
#include "config/validateConfig.hpp"
#include "network/SocketManager.hpp"
#include "utils/printInfo.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <string>

namespace {
inline constexpr std::string_view DEFAULT_CONFIG_PATH{"./configs/default.conf"};

bool isValidConfigFile(const std::string& path) {
    std::filesystem::path p(path);
    return std::filesystem::is_regular_file(p) && p.extension() == ".conf";
}

std::string resolveConfigPath(int argc, char** argv) {
    std::string config_path;
    if (argc == 1) {
        config_path = DEFAULT_CONFIG_PATH;
    } else if (argc == 2) {
        config_path = argv[1];
        if (!isValidConfigFile(config_path)) {
            throw std::runtime_error("Error: Configuration file must exist and end with '.conf'");
        }
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