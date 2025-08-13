/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webserv.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/20 21:57:56 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/13 09:41:11 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    webserv.cpp
 * @brief   Webserv bootstrap and runtime orchestration.
 *
 * @details Resolves the configuration path, loads and parses the configuration file,
 *          applies normalization and validation, prints the effective config, and
 *          starts the socket event loop. Helper functions for argument handling and
 *          file I/O are kept internal to this translation unit.
 * @ingroup core
 */

#include "config/Config.hpp"
#include "config/normalizeConfig.hpp"
#include "config/parser/ConfigParser.hpp"
#include "config/validateConfig.hpp"
#include "network/SocketManager.hpp"
#include "utils/printInfo.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

/**
 * @brief Default configuration file path used when no CLI argument is provided.
 * @internal
 */
inline constexpr std::string_view DEFAULT_CONFIG_PATH{"./configs/default.conf"};

/**
 * @brief Resolves the configuration file path from CLI arguments.
 *
 * @details Accepts either zero or one user-supplied path. When no path is provided,
 *          the function returns the default path specified by `DEFAULT_CONFIG_PATH`.
 *          Passing more than one non-program argument is considered an error.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return The resolved configuration file path.
 * @throws std::runtime_error If more than one configuration path is supplied.
 * @internal
 */
std::string resolveConfigPath(int argc, char** argv) {
    std::string config_path;
    if (argc == 1) {
        config_path = std::string(DEFAULT_CONFIG_PATH);
    } else if (argc == 2) {
        config_path = argv[1];
    } else if (argc > 2) {
        throw std::runtime_error(printUsage());
    }
    return config_path;
}

/**
 * @brief Reads the entire configuration file into memory.
 *
 * @param config_path Path to the configuration file.
 * @return The file contents as a single string.
 * @throws std::runtime_error If the file cannot be opened.
 * @internal
 */
std::string extractFileContent(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file) {
        throw std::runtime_error("Failed to open config file: " + config_path);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

/**
 * @brief Parses a configuration string into a Config object.
 *
 * @param fileContent Raw configuration text.
 * @return Parsed configuration.
 * @throws ConfigParseError On lexical/syntax errors in the configuration text.
 * @internal
 */
Config loadConfig(const std::string& fileContent) {
    ConfigParser parser(fileContent);
    return parser.parseConfig();
}

} // namespace

/**
 * @brief Runs the Webserv server.
 *
 * @details Bootstraps the application by resolving the config path, loading and parsing
 *          the configuration, normalizing defaults, validating constraints, and launching
 *          the non-blocking socket manager event loop.
 *
 * @param argc Argument count.
 * @param argv Argument vector. Optional: path to the configuration file.
 * @return `EXIT_SUCCESS` on clean shutdown; `EXIT_FAILURE` is returned by `main()` on errors.
 * @throws std::runtime_error If CLI arguments are invalid or the config file cannot be opened.
 * @throws ConfigParseError   If the configuration cannot be tokenized/parsed.
 * @throws ValidationError    If the resulting configuration fails validation rules.
 */
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