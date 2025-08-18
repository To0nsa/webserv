/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   directive_handler_table.cpp                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/08 17:14:27 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 19:35:02 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    directive_handler_table.cpp
 * @brief   Implements directive handler maps for `server` and `location` blocks.
 *
 * @details Defines the concrete handler tables returned by
 *          directive::serverHandlers and directive::locationHandlers.
 *          Each handler validates its arguments, reports precise diagnostics
 *          (line/column + contextual snippet), and mutates the target
 *          configuration object (Server or Location).
 *
 *          This file also contains small internal helpers used by handlers:
 *          - `resolveToAbsolute()` — canonicalizes filesystem paths.
 *          - `requireArgCount()` / `requireMinArgCount()` — arity checks.
 *          - `validateIPv4Address()` — IPv4 format validation (with `localhost` allowed).
 *          - `validateCgiExtension()` — enforces dot-prefixed alnum extensions.
 *
 * @ingroup config_parsing
 */

#include "config/parser/directive_handler_table.hpp"
#include "config/parser/ConfigParseError.hpp"
#include "core/Location.hpp"
#include "core/Server.hpp"
#include "utils/errorUtils.hpp"
#include "utils/stringUtils.hpp"

#include <filesystem>
#include <set>
#include <sstream>
#include <unistd.h>

namespace directive {

/**
 * @brief Resolves a filesystem path to its absolute, normalized form.
 *
 * @details Converts the given raw path string into an absolute path
 *          using `std::filesystem::absolute()`, then applies lexical
 *          normalization (removing redundant `.` and `..` components
 *          and duplicate separators). This ensures the result is a
 *          canonicalized path string safe for use in configuration
 *          directives (e.g., `root`, `upload_store`).
 *
 * @param rawPath The raw input path (may be relative or contain `.`/`..`).
 * @return A string representing the absolute, lexically normalized path.
 *
 * @throw std::filesystem::filesystem_error If the path cannot be resolved
 *        due to invalid characters or inaccessible filesystem.
 *
 * @ingroup config_parser
 */
static std::string resolveToAbsolute(const std::string& rawPath) {
    std::filesystem::path abs = std::filesystem::absolute(rawPath);
    return abs.lexically_normal().string();
}

/**
 * @brief Ensures that a directive has exactly the expected number of arguments.
 *
 * @details Used during configuration parsing to validate that a directive
 *          (e.g., `listen`, `root`, `client_max_body_size`) is provided with
 *          the correct number of arguments. If the count does not match
 *          the expectation, a `SyntaxError` is thrown with contextual
 *          information about the directive, line, column, and source snippet.
 *
 * @param args       The list of argument strings parsed for the directive.
 * @param expected   The exact number of arguments required.
 * @param directive  The name of the directive being validated.
 * @param line       The line number in the config file where the directive appears.
 * @param column     The column number in the config file where the directive appears.
 * @param ctx        A snippet of the surrounding source text for error reporting.
 *
 * @throw SyntaxError If the directive does not have exactly `expected` arguments.
 *
 * @ingroup config_parser
 */
static void requireArgCount(const std::vector<std::string>& args, std::size_t expected,
                            const std::string& directive, int line, int column,
                            const std::string& ctx) {
    // Check if the argument count matches the expected number
    if (args.size() != expected) {
        // If not, throw a SyntaxError with detailed context
        throw SyntaxError(formatError("Directive '" + directive + "' takes exactly " +
                                          std::to_string(expected) + " argument(s), but got " +
                                          std::to_string(args.size()),
                                      line, column),
                          ctx);
    }
}

/**
 * @brief Ensures that a directive has at least the required number of arguments.
 *
 * @details Used during configuration parsing to validate directives that
 *          accept a variable number of arguments (e.g., `server_name`,
 *          `error_page`, `index`). If the number of arguments is below
 *          the minimum threshold, a `SyntaxError` is thrown with detailed
 *          information about the directive, its location, and the input context.
 *
 * @param args       The list of arguments parsed for the directive.
 * @param min        The minimum number of arguments required.
 * @param directive  The name of the directive being validated.
 * @param line       The line number in the config file where the directive appears.
 * @param column     The column number in the config file where the directive appears.
 * @param ctx        A snippet of the surrounding config source text for context.
 *
 * @throw SyntaxError If the directive has fewer than `min` arguments.
 *
 * @ingroup config_parser
 */
static void requireMinArgCount(const std::vector<std::string>& args, std::size_t min,
                               const std::string& directive, int line, int column,
                               const std::string& ctx) {
    // Check if number of parsed arguments is below the minimum
    if (args.size() < min) {
        // Throw a SyntaxError with detailed error message and context
        throw SyntaxError(formatError("Directive '" + directive + "' requires at least " +
                                          std::to_string(min) + " argument(s), but got " +
                                          std::to_string(args.size()),
                                      line, column),
                          ctx);
    }
}

/**
 * @brief Validates that a given string is a valid IPv4 address.
 *
 * @details This function ensures that the provided `ip` string is either
 *          `"localhost"` (special case allowed) or a well-formed IPv4 address
 *          in dotted-decimal notation (e.g., `127.0.0.1`, `192.168.1.42`).
 *          It splits the input into octets, parses each into an integer, and
 *          validates that:
 *          - There are exactly 4 octets.
 *          - Each octet is between 0 and 255.
 *          - No extra segments exist.
 *
 * @param ip                The IP address string to validate.
 * @param line              Line number in the config file where the directive appears.
 * @param column            Column number in the config file where the directive appears.
 * @param context_provider  Lazy-evaluated function returning a source context snippet.
 *
 * @throw SyntaxError If the IP address is malformed (wrong number of octets,
 *                    non-numeric values, or values outside 0–255).
 *
 * @ingroup config_parser
 */
static void validateIPv4Address(const std::string& ip, int line, int column,
                                const std::function<std::string()>& context_provider) {
    std::istringstream iss(ip);
    std::string        segment;
    int                count = 0;

    // Special case: allow "localhost" without further validation
    if (ip == "localhost")
        return;

    // Split the string by '.' and validate each segment
    while (std::getline(iss, segment, '.')) {
        if (++count > 4) {
            // More than 4 parts → invalid IPv4
            throw SyntaxError(formatError("Too many octets in IP address: " + ip, line, column),
                              context_provider());
        }

        // Convert segment to integer, throws if non-numeric
        int octet = parseInt(segment, "host", line, column, context_provider);

        // Check range validity (0–255)
        if (octet < 0 || octet > 255) {
            throw SyntaxError(
                formatError("Invalid IP octet '" + segment + "' in host: " + ip, line, column),
                context_provider());
        }
    }

    // Must have exactly 4 octets
    if (count != 4) {
        throw SyntaxError(
            formatError("Invalid IP address format (expected 4 octets): " + ip, line, column),
            context_provider());
    }
}

/**
 * @brief Validates the syntax of a CGI file extension.
 *
 * @details This function ensures that a given CGI extension string is valid.
 *          A valid extension must:
 *          - Be non-empty.
 *          - Begin with a dot (`.`).
 *          - Contain only alphanumeric characters after the dot.
 *          - Not be just `"."` with no following characters.
 *
 * @param ext               The extension string to validate (e.g., ".php").
 * @param line              Line number in the config file for error reporting.
 * @param column            Column number in the config file for error reporting.
 * @param context_provider  Lazy-evaluated function returning a source context snippet.
 *
 * @throw SyntaxError If the extension is malformed or contains invalid characters.
 *
 * @ingroup config_parser
 */
static void validateCgiExtension(const std::string& ext, int line, int column,
                                 const std::function<std::string()>& context_provider) {
    // Must not be empty, a lone '.', and must start with a dot
    if (ext.empty() || ext == "." || ext[0] != '.') {
        throw SyntaxError(formatError("Invalid CGI extension: '" + ext + "'", line, column),
                          context_provider());
    }

    // Check that all characters after '.' are alphanumeric
    for (std::size_t i = 1; i < ext.size(); ++i) {
        char c = ext[i];
        if (!std::isalnum(c)) {
            throw SyntaxError(
                formatError("Invalid character in CGI extension: '" + ext + "'", line, column),
                context_provider());
        }
    }
}

/**
 * @brief Returns the mapping of server-level directives to their handler functions.
 *
 * @details
 * This function builds (once, statically) an `unordered_map` that associates each
 * directive keyword valid inside a `server { ... }` block with a corresponding
 * validation and handler lambda. Each handler is responsible for:
 *   - Checking argument count and validity.
 *   - Validating the argument format (IP, port, byte sizes, etc.).
 *   - Updating the `Server` object with the parsed value.
 *
 * Supported directives:
 * - **listen <port>;**
 *   Validates port number (0–65535) and stores it in the server object.
 * - **host <ip>;**
 *   Validates IPv4 address (or "localhost") and assigns it to the server.
 * - **server_name <name...>;**
 *   Accepts one or more hostnames; each is registered as a valid server name.
 * - **client_max_body_size <size>;**
 *   Validates a byte-size expression (e.g., "10M") and sets the upload limit.
 * - **error_page <code...> <uri>;**
 *   Accepts one or more HTTP error codes followed by a URI.
 *   Maps each error code to the provided URI.
 *
 * @return A reference to the immutable directive-to-handler map.
 *
 * @ingroup config_parser
 */
const std::unordered_map<std::string, ServerHandler>& serverHandlers() {

    // Static so the map is created once and reused on subsequent calls.
    static const std::unordered_map<std::string, ServerHandler> map = {
        // ───────────────────────────────
        // `listen <port>;`
        // Validates that the port number is within range [0, 65535].
        {"listen",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 1, "listen", line, column, ctx); // must have exactly one argument
             int port = parseInt(v[0], "listen", line, column, [&]() { return ctx; });
             if (port < 0 || port > 65535) {
                 throw SyntaxError(
                     formatError("Port number out of valid range (0-65535): " + v[0], line, column),
                     ctx);
             }
             s.setPort(port); // assign validated port to server
         }},

        // ───────────────────────────────
        // `host <ip>;`
        // Validates IPv4 address (or "localhost").
        {"host",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 1, "host", line, column, ctx); // must have exactly one argument
             const std::string& ip = v[0];
             validateIPv4Address(ip, line, column, [&]() { return ctx; }); // ensure valid format
             s.setHost(ip);                                                // store host in server
         }},

        // ───────────────────────────────
        // `server_name <name...>;`
        // Allows one or more server names (aliases).
        {"server_name",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             requireMinArgCount(v, 1, "server_name", line, column, ctx); // at least one name
             for (const auto& name : v) {
                 s.addServerName(name); // register each alias
             }
         }},

        // ───────────────────────────────
        // `client_max_body_size <size>;`
        // Validates a byte-size string (e.g., "10M") and sets upload limit.
        {"client_max_body_size",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 1, "client_max_body_size", line, column, ctx); // one argument
             s.setClientMaxBodySize(parseByteSize(v[0], "client_max_body_size", line, column,
                                                  [&]() { return ctx; })); // convert and set
         }},

        // ───────────────────────────────
        // `error_page <code...> <uri>;`
        // Maps one or more error codes to a redirect URI.
        {"error_page",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             requireMinArgCount(v, 2, "error_page", line, column, ctx); // at least one code + URI
             std::string uri = v.back();                                // last argument is the URI
             for (std::size_t i = 0; i + 1 < v.size(); ++i) {
                 int code = parseInt(v[i], "error_page", line, column, [&]() { return ctx; });
                 s.setErrorPage(code, uri); // map each code to the URI
             }
         }},
    };

    return map;
}

/**
 * @brief Returns the mapping of location-level directives to their handler functions.
 *
 * @details
 * This function builds (once, statically) an `unordered_map` that associates each
 * directive keyword valid inside a `location { ... }` block with its validation
 * and handler lambda. Each handler checks argument counts, validates syntax,
 * and updates the `Location` object accordingly.
 *
 * Supported directives:
 * - **root <path>;**
 *   Sets the filesystem root for this location (absolute, normalized path).
 * - **index <file...>;**
 *   Specifies one or more default index files (supports comma-separated values).
 * - **autoindex on|off;**
 *   Enables or disables directory listing.
 * - **methods <HTTP-method...>;**
 *   Restricts allowed HTTP methods (GET, POST, DELETE).
 * - **upload_store <path>;**
 *   Defines the directory for file uploads (absolute, normalized path).
 * - **cgi_extension <.ext...>;**
 *   Lists extensions handled by CGI (e.g., `.php`, `.py`).
 * - **cgi_interpreter <.ext> <path>;**
 *   Associates a CGI extension with an executable interpreter.
 * - **return <code> <uri>;**
 *   Configures a redirect with HTTP status code.
 *
 * @return A reference to the immutable directive-to-handler map.
 *
 * @ingroup config_parser
 */
const std::unordered_map<std::string, LocationHandler>& locationHandlers() {
    static const std::unordered_map<std::string, LocationHandler> map = {
        // ───────────────────────────────
        // `root <path>;`
        // Sets the root directory for this location.
        {"root",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 1, "root", line, column, ctx); // must have exactly one path
             loc.setRoot(resolveToAbsolute(v[0]));             // normalize to absolute path
         }},

        // ───────────────────────────────
        // `index <file...>;`
        // Defines one or more index files (comma-separated allowed).
        {"index",
         [](Location& loc, const auto& args, int line, int column, const std::string& ctx) {
             requireMinArgCount(args, 1, "index", line, column, ctx); // at least one file
             for (const std::string& raw : args) {
                 size_t start = 0, end;
                 // Split on commas
                 while ((end = raw.find(',', start)) != std::string::npos) {
                     std::string idx = raw.substr(start, end - start);
                     if (!idx.empty())
                         loc.addIndexFile(idx); // add each file
                     start = end + 1;
                 }
                 std::string idx = raw.substr(start);
                 if (!idx.empty())
                     loc.addIndexFile(idx);
             }
         }},

        // ───────────────────────────────
        // `autoindex on|off;`
        // Enables or disables directory listing.
        {"autoindex",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 1, "autoindex", line, column, ctx);
             if (v[0] == "on") {
                 loc.setAutoindex(true);
             } else if (v[0] == "off") {
                 loc.setAutoindex(false);
             } else {
                 throw SyntaxError(
                     formatError("Invalid value for 'autoindex': " + v[0], line, column), ctx);
             }
         }},

        // ───────────────────────────────
        // `methods <method...>;`
        // Restricts allowed HTTP methods (must be GET, POST, or DELETE).
        {"methods",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             if (v.empty()) {
                 throw SyntaxError(
                     formatError("Directive 'methods' requires at least one HTTP method", line,
                                 column),
                     ctx);
             }
             static const std::set<std::string> valid_methods = {"GET", "POST", "DELETE"};
             for (const auto& m : v) {
                 if (!valid_methods.count(m)) {
                     throw SyntaxError(formatError("Invalid HTTP method: " + m, line, column), ctx);
                 }
                 loc.addMethod(m); // store validated method
             }
         }},

        // ───────────────────────────────
        // `upload_store <path>;`
        // Sets the directory where file uploads will be saved.
        {"upload_store",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 1, "upload_store", line, column, ctx);
             loc.setUploadStore(resolveToAbsolute(v[0])); // normalize path
         }},

        // ───────────────────────────────
        // `cgi_extension <.ext...>;`
        // Declares CGI file extensions (e.g., `.php`).
        {"cgi_extension",
         [](Location& loc, const auto& args, int line, int column, const std::string& ctx) {
             requireMinArgCount(args, 1, "cgi_extension", line, column, ctx);
             for (const std::string& raw : args) {
                 size_t start = 0, end;
                 // Split multiple extensions separated by commas
                 while ((end = raw.find(',', start)) != std::string::npos) {
                     std::string ext = raw.substr(start, end - start);
                     if (!ext.empty()) {
                         validateCgiExtension(ext, line, column, [&]() { return ctx; });
                         loc.addCgiExtension(ext);
                     }
                     start = end + 1;
                 }
                 std::string ext = raw.substr(start);
                 if (!ext.empty()) {
                     validateCgiExtension(ext, line, column, [&]() { return ctx; });
                     loc.addCgiExtension(ext);
                 }
             }
         }},

        // ───────────────────────────────
        // `cgi_interpreter <.ext> <path>;`
        // Maps a CGI extension to its interpreter binary.
        {"cgi_interpreter",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 2, "cgi_interpreter", line, column, ctx); // ext + path required
             const std::string& ext  = v[0];
             const std::string& path = v[1];

             validateCgiExtension(ext, line, column, [&]() { return ctx; }); // must be valid ext
             if (!std::filesystem::is_regular_file(path) || access(path.c_str(), X_OK) != 0) {
                 throw SyntaxError(
                     formatError("Interpreter not executable or not found: " + path, line, column),
                     ctx);
             }
             if (!loc.getCgiInterpreter(ext).empty()) {
                 throw SyntaxError(
                     formatError("Duplicate cgi_interpreter for " + ext + ": already defined", line,
                                 column),
                     ctx);
             }
             loc.addCgiInterpreter(ext, path); // store extension → interpreter mapping
         }},

        // ───────────────────────────────
        // `return <code> <uri>;`
        // Defines a redirect (e.g., `return 301 /newpath;`).
        {"return",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 2, "return", line, column, ctx);
             int code = parseInt(v[0], "return", line, column, [&]() { return ctx; });
             loc.setRedirect(v[1], code); // assign redirect URI + status code
         }},
    };

    return map;
}

} // namespace directive
