/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigValidator.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 00:11:16 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/11 23:03:23 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    ConfigValidator.hpp
 * @brief   Defines the ConfigValidator class responsible for static configuration validation.
 *
 * @details This header declares the `ConfigValidator` class, which provides a comprehensive
 *          validation pass over the parsed `Config` structure. It enforces semantic correctness
 *          and internal consistency by applying a series of rule-based checks on each `Server`
 *          and `Location` block. These include validation of port uniqueness, allowed HTTP methods,
 *          redirect codes, upload paths, CGI settings, and more.
 *
 *          This validator is designed to be called once after parsing and normalization, and
 *          throws `SyntaxError` exceptions on any misconfiguration.
 *
 * @ingroup config
 */

#pragma once

#include "config/Config.hpp"
#include <vector>

/**
 * @brief Performs semantic validation on parsed configuration data.
 *
 * @details The `ConfigValidator` class runs a series of rule-based checks over the parsed
 *          configuration (`Config`) to ensure all `Server` and `Location` blocks are
 *          semantically valid and safe to execute. It validates things like:
 *          - Unique `host:port` + `server_name` combinations
 *          - Valid HTTP methods and redirect codes
 *          - Correct structure of `upload_store`, `cgi_extension`, and `error_page`
 *          - Required presence of `location` blocks, `root`, and allowed methods
 *
 *          This validator is typically invoked after parsing and before runtime initialization.
 *
 * @ingroup config
 */
class ConfigValidator {
  public:
    /**
     * @brief Runs all validation checks on the given configuration.
     *
     * @details This method applies a full validation pass over all `Server` and `Location`
     *          blocks in the provided `Config` object. It enforces uniqueness, completeness,
     *          and compliance with expected constraints (method validity, error codes,
     *          body size limits, etc.). Throws on first encountered violation.
     *
     * @param config Parsed configuration object to validate.
     *
     * @throws SyntaxError If any rule is violated in the configuration.
     */
    void validate(const Config& config);

  private:
    /**
     * @brief Ensures each server block contains at least one location block.
     *
     * @details The server cannot function without any `location` directives. This check throws
     *          an error if a server block is defined with no associated `location` blocks.
     *
     * @param servers List of parsed servers to validate.
     *
     * @throws SyntaxError If any server lacks at least one location block.
     */
    void validateHasLocation(const std::vector<Server>& servers);
    /**
     * @brief Validates uniqueness of virtual host definitions per host:port.
     *
     * @details Ensures that for each `(host, port)` combination, all `server_name` values are
     * unique. If no `server_name` is provided, the server is treated as the default and only one
     *          default is allowed per host:port pair.
     *
     * @param servers List of servers to validate.
     *
     * @throws SyntaxError If two servers share the same host, port, and server_name.
     */
    void validateUniquePorts(const std::vector<Server>& servers);
    /**
     * @brief Validates the minimal structure of each location block.
     *
     * @details Ensures each location has either a `root` or a `return` directive, but not both
     *          `return` and `cgi_extension`. Also verifies that at least one HTTP method is
     * declared.
     *
     * @param servers List of servers whose locations are to be checked.
     *
     * @throws SyntaxError If a location is structurally incomplete or contains conflicting
     * directives.
     */
    void validateLocationDefaults(const std::vector<Server>& servers);
    /**
     * @brief Ensures no duplicate server names within a single server block.
     *
     * @details Verifies that each `server_name` listed in a server block is unique. This is a
     *          per-server check and does not compare across servers (that's done in
     * `validateUniquePorts`).
     *
     * @param servers List of servers to validate.
     *
     * @throws SyntaxError If a server block declares the same server_name more than once.
     */
    void validateUniqueServerNames(const std::vector<Server>& servers);
    /**
     * @brief Validates that all error_page codes are within the valid HTTP error range.
     *
     * @details Checks that each error code specified in `error_page` directives falls within
     *          the range [400, 599], as required by the HTTP specification.
     *
     * @param servers List of servers to validate.
     *
     * @throws SyntaxError If an invalid error code is found.
     */
    void validateErrorPageCodes(const std::vector<Server>& servers);
    /**
     * @brief Validates that all redirect codes are valid HTTP 3xx status codes.
     *
     * @details Checks each `return` directive in location blocks to ensure the status code
     *          is between 300 and 399. These are the only codes valid for HTTP redirection.
     *
     * @param servers List of servers whose locations are to be validated.
     *
     * @throws SyntaxError If a redirect code is outside the 300–399 range.
     */
    void validateRedirectCodes(const std::vector<Server>& servers);
    /**
     * @brief Validates that all declared HTTP methods are standard and recognized.
     *
     * @details Ensures that each method specified in a `methods` directive is one of the
     *          supported HTTP verbs (e.g., GET, POST, DELETE, etc.). This prevents typos
     *          or unsupported methods from silently passing through.
     *
     * @param servers List of servers to validate.
     *
     * @throws SyntaxError If an unknown or invalid HTTP method is encountered.
     */
    void validateAllowedMethods(const std::vector<Server>& servers);
    /**
     * @brief Validates that the client_max_body_size directive is strictly positive.
     *
     * @details Ensures that each server declares a `client_max_body_size` greater than zero.
     *          A value of zero disables request body handling, which is disallowed here for
     * correctness.
     *
     * @param servers List of servers to validate.
     *
     * @throws SyntaxError If any server sets a body size limit of zero.
     */
    void validateClientMaxBodySize(const std::vector<Server>& servers);
    /**
     * @brief Validates the correctness and safety of upload_store paths.
     *
     * @details Ensures that if `upload_store` is enabled in a location, its path:
     *          - is non-empty,
     *          - is absolute (starts with '/'),
     *          - does not contain directory traversal sequences (e.g., "..").
     *
     * @param servers List of servers to validate.
     *
     * @throws SyntaxError If an upload_store path is invalid or unsafe.
     */
    void validateUploadStorePaths(const std::vector<Server>& servers);
    /**
     * @brief Validates CGI extensions declared in each location block.
     *
     * @details Ensures that each CGI extension starts with a dot (`.`), is not empty,
     *          and is not just a single dot. This prevents malformed or ambiguous
     *          file extension matching.
     *
     * @param servers List of servers to validate.
     *
     * @throws SyntaxError If any CGI extension is invalid or improperly formatted.
     */
    void validateCgiExtensions(const std::vector<Server>& servers);
};
