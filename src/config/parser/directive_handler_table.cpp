/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   directive_handler_table.cpp                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/08 17:14:27 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/09 08:42:35 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    directive_handler_table.cpp
 * @brief   Defines handler dispatch tables for server and location directives.
 *
 * @details This file implements the dispatch tables that map configuration directives
 *          (such as `listen`, `host`, `server_name`, etc.) to their respective handler
 *          functions (`ServerHandler` and `LocationHandler`). These handler functions
 *          are responsible for parsing and applying configuration values to the `Server`
 *          and `Location` objects. The tables are used by the `ConfigParser` during
 *          configuration file parsing, allowing for efficient and extensible handling of
 *          various server and location configuration options.
 *
 * @ingroup config
 */

#include "config/parser/directive_handler_table.hpp"
#include "config/parser/ConfigParseError.hpp"
#include "core/Location.hpp"
#include "core/Server.hpp"
#include "utils/errorUtils.hpp"
#include "utils/stringUtils.hpp"

#include <set>

/**
 * @namespace directive
 * @brief Contains functions and handlers for parsing and processing configuration directives.
 *
 * @details The `directive` namespace groups functions and handler tables responsible for managing
 *          the server and location configuration directives within the web server configuration
 * file. These functions map directives (e.g., `listen`, `root`, `index`) to their respective
 * handlers which modify the `Server` and `Location` objects during the parsing process. By
 * encapsulating directive-specific logic in this namespace, we maintain modularity, avoid name
 * clashes, and facilitate easy expansion for handling new configuration options.
 *
 * @ingroup config
 */
namespace directive {

static void requireArgCount(const std::vector<std::string>& args, std::size_t expected,
                            const std::string& directive, int line, int column,
                            const std::string& contextWindow) {
    // Check if the number of arguments matches the expected count for the directive
    if (args.size() != expected) {
        // If not, throw a SyntaxError with a descriptive message
        throw SyntaxError(formatError("Directive '" + directive + "' takes exactly " +
                                          std::to_string(expected) + " argument(s), but got " +
                                          std::to_string(args.size()),
                                      line, column),
                          contextWindow);
    }
}

static void requireMinArgCount(const std::vector<std::string>& args, std::size_t min,
                               const std::string& directive, int line, int column,
                               const std::string& ctx) {
    // Check if the number of arguments is less than the minimum required for the directive
    if (args.size() < min) {
        // If not, throw a SyntaxError with a descriptive message
        throw SyntaxError(formatError("Directive '" + directive + "' requires at least " +
                                          std::to_string(min) + " argument(s), but got " +
                                          std::to_string(args.size()),
                                      line, column),
                          ctx);
    }
}

const std::unordered_map<std::string, ServerHandler>& serverHandlers() {
    // Define a static unordered_map to hold server directive handlers.
    // The map keys are directive names (e.g., "listen", "host"), and the values are
    // corresponding handler functions that will process the directive arguments
    // and apply them to the Server object.
    static const std::unordered_map<std::string, ServerHandler> map = {
        // Handle the "listen" directive, which defines the port number the server will bind to.
        // The handler checks that exactly one argument is provided (the port number).
        // The port is parsed, and errors are raised if the port is invalid, out of range,
        // or if the input cannot be converted to an integer. The port number must be between
        // 0 and 65535. Once validated, the port is set on the Server object using setPort().
        {"listen",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             // Ensure exactly one argument is provided for the "listen" directive
             requireArgCount(v, 1, "listen", line, column, ctx);
             int port;
             try {
                 // Attempt to convert the argument to an integer
                 port = parseInt(v[0]);
             } catch (const std::invalid_argument&) {
                 // Catch invalid numeric conversion (e.g. "abc")
                 throw SyntaxError(formatError("Invalid port number: " + v[0], line, column), ctx);
             } catch (const std::out_of_range&) {
                 // Catch number too large or small for int
                 throw SyntaxError(
                     formatError("Port number out of integer range: " + v[0], line, column), ctx);
             }
             if (port < 0 || port > 65535) {
                 // Validate the port is within allowed range
                 throw SyntaxError(
                     formatError("Port number out of valid range (0-65535): " + v[0], line, column),
                     ctx);
             }
             s.setPort(port); // Set the validated port on the server object
         }},
        // Handle the "host" directive, which defines the server's binding address.
        // The handler checks that exactly one argument is provided (the host IP address),
        // and then sets it on the Server object using the setHost() method.
        {"host",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             // Ensure exactly one argument is provided for the "host" directive
             requireArgCount(v, 1, "host", line, column, ctx);
             // Set the host IP address on the Server object
             s.setHost(v[0]);
         }},
        // Handle the "server_name" directive, which defines the aliases for the server's name.
        // The handler ensures that at least one argument is provided (the server name),
        // and then adds each provided name to the Server object using the addServerName() method.
        {"server_name",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             // Ensure at least one argument is provided for the "server_name" directive
             requireMinArgCount(v, 1, "server_name", line, column, ctx);
             // Add each provided server name to the Server object
             for (const auto& name : v) {
                 s.addServerName(name);
             }
         }},
        // Handle the "client_max_body_size" directive, which sets the maximum allowed body size for
        // client requests.
        // The handler ensures exactly one argument is provided (the size), and then parses the size
        // using parseByteSize().
        // The parsed size is then set on the Server object using setClientMaxBodySize().
        {"client_max_body_size",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             // Ensure exactly one argument is provided for the "client_max_body_size" directive
             requireArgCount(v, 1, "client_max_body_size", line, column, ctx);

             // Parse the byte size and set it on the Server object
             s.setClientMaxBodySize(parseByteSize(v[0]));
         }},
        // Handle the "error_page" directive, which maps HTTP error codes to custom error page URIs.
        // The handler ensures at least two arguments are provided (error code and URI).
        // The error codes are parsed and mapped to the provided URI using setErrorPage().
        {"error_page",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             // Ensure at least two arguments are provided for the "error_page" directive
             requireMinArgCount(v, 2, "error_page", line, column, ctx);

             // The last argument is the URI for the error page
             std::string uri = v.back();

             // Iterate over the error codes and set the custom error page for each
             for (std::size_t i = 0; i + 1 < v.size(); ++i) {
                 int code = parseInt(v[i]);
                 s.setErrorPage(code, uri); // Set the error page for each code
             }
         }},
    };
    return map;
}

const std::unordered_map<std::string, LocationHandler>& locationHandlers() {
    // Define a static unordered_map to hold location directive handlers.
    // The map keys are directive names (e.g., "root", "index"), and the values are
    // corresponding handler functions that will process the directive arguments
    // and apply them to the Location object.
    static const std::unordered_map<std::string, LocationHandler> map = {
        // Handle the "root" directive, which defines the root directory for this location.
        // The handler ensures exactly one argument is provided (the root directory path),
        // and then sets it on the Location object using setRoot().
        {"root",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             // Ensure exactly one argument is provided for the "root" directive
             requireArgCount(v, 1, "root", line, column, ctx);
             // Set the root directory path on the Location object
             loc.setRoot(v[0]);
         }},
        // Handle the "index" directive, which defines the index file(s) for this location.
        // The handler ensures at least one argument is provided (the index files).
        // The index files can be comma-separated, and each is added to the Location object
        // using addIndexFile().
        {"index",
         [](Location& loc, const auto& args, int line, int column, const std::string& ctx) {
             // Ensure at least one argument is provided for the "index" directive
             requireMinArgCount(args, 1, "index", line, column, ctx);
             // Process each index file (can be comma-separated) and add it to the Location object
             for (const std::string& raw : args) {
                 size_t start = 0, end;
                 while ((end = raw.find(',', start)) != std::string::npos) {
                     std::string idx = raw.substr(start, end - start);
                     if (!idx.empty())
                         loc.addIndexFile(idx); // Add the index file to the Location object
                     start = end + 1;
                 }
                 std::string idx = raw.substr(start);
                 if (!idx.empty())
                     loc.addIndexFile(idx); // Add the last index file if exists
             }
         }},
        // Handle the "autoindex" directive, which enables or disables directory autoindexing.
        // The handler ensures exactly one argument is provided (either "on" or "off").
        // The autoindexing flag is set on the Location object using setAutoindex().
        // If an invalid value is provided, a SyntaxError is thrown.
        {"autoindex",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             // Ensure exactly one argument is provided for the "autoindex" directive
             requireArgCount(v, 1, "autoindex", line, column, ctx);
             // Enable or disable autoindexing based on the provided argument
             if (v[0] == "on") {
                 loc.setAutoindex(true); // Enable autoindexing
             } else if (v[0] == "off") {
                 loc.setAutoindex(false); // Disable autoindexing
             } else {
                 // If an invalid value is provided, throw a syntax error
                 throw SyntaxError(
                     formatError("Invalid value for 'autoindex': " + v[0], line, column), ctx);
             }
         }},
        // Handle the "methods" directive, which defines the allowed HTTP methods for this location.
        // The handler ensures at least one method is provided, and each method is validated against
        // a predefined set of valid methods (e.g., "GET", "POST", "DELETE"). Invalid methods
        // trigger
        // a SyntaxError. Valid methods are added to the Location object using addMethod().
        {"methods",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             // Ensure at least one method is provided for the "methods" directive
             if (v.empty()) {
                 throw SyntaxError(
                     formatError("Directive 'methods' requires at least one HTTP method", line,
                                 column),
                     ctx);
             }
             // Define the set of valid HTTP methods
             static const std::set<std::string> valid_methods = {
                 "GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE", "PATCH"};
             // Iterate through each provided method, validating and adding them
             for (const auto& m : v) {
                 // Check if the method is valid, otherwise throw an error
                 if (!valid_methods.count(m)) {
                     throw SyntaxError(formatError("Invalid HTTP method: " + m, line, column), ctx);
                 }
                 // Add the valid method to the Location object
                 loc.addMethod(m);
             }
         }},
        // Handle the "upload_store" directive, which defines the directory where uploaded files
        // should be stored for this location. The handler ensures exactly one argument is provided
        // (the directory path), and then sets it on the Location object using setUploadStore().
        {"upload_store",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             // Ensure exactly one argument is provided for the "upload_store" directive
             requireArgCount(v, 1, "upload_store", line, column, ctx);
             // Set the upload directory path on the Location object
             loc.setUploadStore(v[0]);
         }},
        // Handle the "cgi_extension" directive, which defines file extensions that should be
        // handled via CGI (e.g., ".php", ".py"). The handler ensures at least one argument is
        // provided, processes the extensions (which can be comma-separated), and adds each valid
        // extension and adds each valid extension to the Location object using addCgiExtension().
        {"cgi_extension",
         [](Location& loc, const auto& args, int line, int column, const std::string& ctx) {
             // Ensure at least one argument is provided for the "cgi_extension" directive
             requireMinArgCount(args, 1, "cgi_extension", line, column, ctx);
             // Process each extension (can be comma-separated) and add it to the Location object
             for (const std::string& raw : args) {
                 size_t start = 0, end;
                 while ((end = raw.find(',', start)) != std::string::npos) {
                     std::string ext = raw.substr(start, end - start);
                     if (!ext.empty())
                         loc.addCgiExtension(ext); // Add the CGI extension to the Location object
                     start = end + 1;
                 }
                 std::string ext = raw.substr(start);
                 if (!ext.empty())
                     loc.addCgiExtension(ext); // Add the last CGI extension if exists
             }
         }},
        // Handle the "return" directive, which defines an HTTP redirection for this location.
        // The handler ensures exactly two arguments are provided (the status code and the
        // redirection URI), parses the status code, and sets the redirection
        // using setRedirect() on the Location object.
        {"return",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             // Ensure exactly two arguments are provided for the "return" directive
             requireArgCount(v, 2, "return", line, column, ctx);
             // Parse the HTTP status code (e.g., 301 for permanent redirection)
             int code = parseInt(v[0]);
             // Set the redirection URI and status code on the Location object
             loc.setRedirect(v[1], code);
         }},
    };
    return map;
}

} // namespace directive
