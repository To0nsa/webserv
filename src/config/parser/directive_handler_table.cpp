/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   directive_handler_table.cpp                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/08 17:14:27 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/14 10:27:30 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

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

static std::string resolveToAbsolute(const std::string& rawPath) {
	std::filesystem::path abs = std::filesystem::absolute(rawPath);
	return abs.lexically_normal().string();
}

static void requireArgCount(const std::vector<std::string>& args, std::size_t expected,
                            const std::string& directive, int line, int column,
                            const std::string& ctx) {
    if (args.size() != expected) {
        throw SyntaxError(formatError("Directive '" + directive + "' takes exactly " +
                                          std::to_string(expected) + " argument(s), but got " +
                                          std::to_string(args.size()),
                                      line, column),
                          ctx);
    }
}

static void requireMinArgCount(const std::vector<std::string>& args, std::size_t min,
                               const std::string& directive, int line, int column,
                               const std::string& ctx) {
    if (args.size() < min) {
        throw SyntaxError(formatError("Directive '" + directive + "' requires at least " +
                                          std::to_string(min) + " argument(s), but got " +
                                          std::to_string(args.size()),
                                      line, column),
                          ctx);
    }
}

static void validateIPv4Address(const std::string& ip, int line, int column,
                                const std::function<std::string()>& context_provider) {
    std::istringstream iss(ip);
    std::string        segment;
    int                count = 0;

    if (ip == "localhost")
        return;

    while (std::getline(iss, segment, '.')) {
        if (++count > 4) {
            throw SyntaxError(formatError("Too many octets in IP address: " + ip, line, column),
                              context_provider());
        }

        int octet = parseInt(segment, "host", line, column, context_provider);

        if (octet < 0 || octet > 255) {
            throw SyntaxError(
                formatError("Invalid IP octet '" + segment + "' in host: " + ip, line, column),
                context_provider());
        }
    }
    if (count != 4) {
        throw SyntaxError(
            formatError("Invalid IP address format (expected 4 octets): " + ip, line, column),
            context_provider());
    }
}

static void validateCgiExtension(const std::string& ext, int line, int column,
                                 const std::function<std::string()>& context_provider) {
    if (ext.empty() || ext == "." || ext[0] != '.') {
        throw SyntaxError(formatError("Invalid CGI extension: '" + ext + "'", line, column),
                          context_provider());
    }

    for (std::size_t i = 1; i < ext.size(); ++i) {
        char c = ext[i];
        if (!std::isalnum(c)) {
            throw SyntaxError(
                formatError("Invalid character in CGI extension: '" + ext + "'", line, column),
                context_provider());
        }
    }
}

const std::unordered_map<std::string, ServerHandler>& serverHandlers() {

    static const std::unordered_map<std::string, ServerHandler> map = {
        {"listen",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 1, "listen", line, column, ctx);
             int port = parseInt(v[0], "listen", line, column, [&]() { return ctx; });
             if (port < 0 || port > 65535) {
                 throw SyntaxError(
                     formatError("Port number out of valid range (0-65535): " + v[0], line, column),
                     ctx);
             }
             s.setPort(port);
         }},
        {"host",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 1, "host", line, column, ctx);
             const std::string& ip = v[0];
             validateIPv4Address(ip, line, column, [&]() { return ctx; });
             s.setHost(ip);
         }},
        {"server_name",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             requireMinArgCount(v, 1, "server_name", line, column, ctx);
             for (const auto& name : v) {
                 s.addServerName(name);
             }
         }},

        {"client_max_body_size",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 1, "client_max_body_size", line, column, ctx);
             s.setClientMaxBodySize(
                 parseByteSize(v[0], "client_max_body_size", line, column, [&]() { return ctx; }));
         }},
        {"error_page",
         [](Server& s, const auto& v, int line, int column, const std::string& ctx) {
             requireMinArgCount(v, 2, "error_page", line, column, ctx);
             std::string uri = v.back();
             for (std::size_t i = 0; i + 1 < v.size(); ++i) {
                 int code = parseInt(v[i], "error_page", line, column, [&]() { return ctx; });
                 s.setErrorPage(code, uri);
             }
         }},
    };
    return map;
}

const std::unordered_map<std::string, LocationHandler>& locationHandlers() {
    static const std::unordered_map<std::string, LocationHandler> map = {
        {"root",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 1, "root", line, column, ctx);
             loc.setRoot(resolveToAbsolute(v[0]));
         }},
        {"index",
         [](Location& loc, const auto& args, int line, int column, const std::string& ctx) {
             requireMinArgCount(args, 1, "index", line, column, ctx);
             for (const std::string& raw : args) {
                 size_t start = 0, end;
                 while ((end = raw.find(',', start)) != std::string::npos) {
                     std::string idx = raw.substr(start, end - start);
                     if (!idx.empty())
                         loc.addIndexFile(idx);
                     start = end + 1;
                 }
                 std::string idx = raw.substr(start);
                 if (!idx.empty())
                     loc.addIndexFile(idx);
             }
         }},
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
                 loc.addMethod(m);
             }
         }},
        {"upload_store",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 1, "upload_store", line, column, ctx);
             loc.setUploadStore(resolveToAbsolute(v[0]));
         }},
        {"cgi_extension",
         [](Location& loc, const auto& args, int line, int column, const std::string& ctx) {
             requireMinArgCount(args, 1, "cgi_extension", line, column, ctx);
             for (const std::string& raw : args) {
                 size_t start = 0, end;
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
        {"cgi_interpreter",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 2, "cgi_interpreter", line, column, ctx);
             const std::string& ext  = v[0];
             const std::string& path = v[1];
             validateCgiExtension(ext, line, column, [&]() { return ctx; });
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
             loc.addCgiInterpreter(ext, path);
         }},
        {"return",
         [](Location& loc, const auto& v, int line, int column, const std::string& ctx) {
             requireArgCount(v, 2, "return", line, column, ctx);
             int code = parseInt(v[0], "return", line, column, [&]() { return ctx; });
             loc.setRedirect(v[1], code);
         }},
    };
    return map;
}

} // namespace directive
