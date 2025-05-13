/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/09 08:46:22 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/09 09:38:32 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    ConfigParser.cpp
 * @brief   Implements the ConfigParser class for parsing configuration files.
 *
 * @details This file defines the `ConfigParser` class methods, responsible for transforming
 *          a raw configuration string into structured `Config`, `Server`, and `Location` objects.
 *          It performs token dispatching, directive validation, duplicate detection,
 *          and error reporting. Directive application is delegated through handler tables
 *          for modular and extensible parsing logic.
 *
 * @ingroup config
 */

#include "config/parser/ConfigParser.hpp"
#include "config/Config.hpp"
#include "config/parser/ConfigParseError.hpp"
#include "config/parser/directive_handler_table.hpp"
#include "config/tokenizer/Tokenizer.hpp"
#include "core/Location.hpp"
#include "core/Server.hpp"
#include "utils/errorUtils.hpp"
#include "utils/stringUtils.hpp"

#include <charconv>
#include <functional>
#include <memory>
#include <sstream>
#include <unordered_map>

namespace {

/// Set of server-level directives that may appear multiple times within a server block.
static const std::set<std::string> kRepeatableServerDirectives = {"error_page"};
/// Set of location-level directives that may appear multiple times within a location block.
static const std::set<std::string> kRepeatableLocationDirectives = {"methods"};
/// Accepted token types for directive arguments (string literals, numbers, or identifiers).
static const std::initializer_list<TokenType> kArgTokenTypes = {
    TokenType::STRING, TokenType::NUMBER, TokenType::IDENTIFIER};

/**
 * @brief Checks whether a directive is a duplicate within its context.
 *
 * @details Determines if the given directive name has already been seen in the current
 *          block (server or location). If the directive is in the repeatable set,
 *          duplicates are allowed. Otherwise, the function inserts the name into the
 *          `seen` set and returns whether it was successfully inserted.
 *
 * @param name        The directive name to check.
 * @param seen        A set tracking already encountered directives.
 * @param repeatable  The set of directives allowed to appear multiple times.
 * @return `true` if the directive is allowed (not a duplicate or is repeatable), `false` otherwise.
 */
bool checkDuplicateDirective(const std::string& name, std::set<std::string>& seen,
                             const std::set<std::string>& repeatable) {
    return repeatable.count(name) || seen.insert(name).second;
}

/**
 * @brief Dispatches and applies a directive using a handler map.
 *
 * @details Looks up the directive name (from the token `key`) in the provided `handlers` map.
 *          If found, invokes the corresponding handler function with the directive arguments.
 *          Converts the directive name to lowercase before lookup to ensure case-insensitivity.
 *          Wraps any handler-side conversion errors (`invalid_argument` or `out_of_range`)
 *          into a `SyntaxError` for uniform error handling during parsing.
 *
 * @tparam T           The target type to apply the directive to (e.g., Server or Location).
 * @tparam HandlerMap  The map type containing directive names and their corresponding handler
 * functions.
 * @param target       The object being modified (Server or Location).
 * @param key          The token representing the directive name.
 * @param values       The list of argument strings for the directive.
 * @param handlers     The directive handler map to use for dispatch.
 * @param line         The line number where the directive appears.
 * @param column       The column number where the directive appears.
 * @param ctx          The contextual snippet used for detailed error reporting.
 *
 * @throws SyntaxError If the directive is unknown or its handler throws a conversion error.
 */
template <typename T, typename HandlerMap>
void parseDirective(T& target, const Token& key, std::vector<std::string>& values,
                    const HandlerMap& handlers, int line, int column, const std::string& ctx) {
    const std::string name = toLower(key.value); // Normalize directive name to lowercase

    // Attempt to find the directive handler in the provided table
    typename HandlerMap::const_iterator it = handlers.find(name);
    if (it == handlers.end()) {
        // If no matching handler, throw a syntax error for unknown directive
        throw SyntaxError(formatError("Unknown directive: '" + key.value + "'", line, column), ctx);
    }

    try {
        // Call the matched handler with the target object and directive arguments
        it->second(target, values, line, column, ctx);
    } catch (const std::invalid_argument& e) {
        // Re-throw std conversion errors as SyntaxError for unified parser error reporting
        throw SyntaxError(formatError(e.what(), line, column), ctx);
    } catch (const std::out_of_range& e) {
        // Handle cases like `stoi()` out-of-bound input
        throw SyntaxError(formatError(e.what(), line, column), ctx);
    }
}

} // namespace

//////////////////
// --- Public API

ConfigParser::ConfigParser(std::string source)
    : // Initialize tokenizer with the raw config string
      _tokenizer(std::move(source)) {
    // Immediately tokenize the input and store the token list
    _tokens = _tokenizer.tokenize();
}

Config ConfigParser::parseConfig() {
    Config config;

    // Reject completely empty configuration input
    if (isAtEnd()) {
        throw SyntaxError(formatError("Empty configuration", 1, 1), getContextWindow());
    }

    // Top-level config must consist of one or more `server` blocks
    while (!isAtEnd()) {
        // If the current token is not `server`, fail early
        if (current().type != TokenType::KEYWORD_SERVER) {
            throw SyntaxError(
                formatError("Expected 'server' block", current().line, current().column),
                getContextWindow());
        }

        // Parse a full server block and append it to the config
        config.addServer(parseServer());

        // After a server block, only EOF or another server block is allowed
        if (!isAtEnd() && current().type != TokenType::KEYWORD_SERVER &&
            current().type != TokenType::END_OF_FILE) {
            throw SyntaxError(formatError("Unexpected token after server block", current().line,
                                          current().column),
                              getContextWindow());
        }
    }

    return config;
}

////////////////////////////
// --- Server Block Parsing

Server ConfigParser::parseServer() {
    expect(TokenType::KEYWORD_SERVER, "server block");  // Ensure block starts with 'server'
    expect(TokenType::LBRACE, "start of server block"); // Expect opening brace '{'

    Server                server;
    std::set<std::string> seen; // Track directives to detect duplicates

    // Loop until closing '}' or end of file
    while (!isAtEnd() && current().type != TokenType::RBRACE) {
        if (current().type == TokenType::KEYWORD_LOCATION) {
            // Parse and attach a location block to the server
            server.addLocation(parseLocation());
        } else {
            // Normalize directive name to lowercase and check for duplicates
            const std::string name = toLower(current().value);
            if (!checkDuplicateDirective(name, seen, kRepeatableServerDirectives)) {
                throw SyntaxError(formatError("Duplicate directive: '" + name + "'", current().line,
                                              current().column),
                                  getContextWindow());
            }
            // Parse the directive and apply it to the server
            parseServerDirective(server);
        }
    }

    expect(TokenType::RBRACE, "end of server block"); // Validate closing '}'
    return server;
}

void ConfigParser::parseServerDirective(Server& server) {
    Token key = current(); // Capture the directive token (e.g., "listen", "host", etc.)
    advance();             // Consume the directive name
    std::vector<std::string> values = collectArgs(kArgTokenTypes);    // Parse directive arguments
    expect(TokenType::SEMICOLON, "semicolon after server directive"); // Enforce `;` terminator
    parseDirective(server, key, values,
                   directive::serverHandlers(), // Dispatch to the correct handler
                   key.line, key.column, getContextWindow());
}

//////////////////////////////
// --- Location Block Parsing

Location ConfigParser::parseLocation() {
    expect(TokenType::KEYWORD_LOCATION, "location block"); // Ensure block starts with 'location'

    Location location;

    // Parse the location path (either a string or an unquoted identifier)
    location.setPath(
        expectOneOf({TokenType::STRING, TokenType::IDENTIFIER}, "location path").value);

    expect(TokenType::LBRACE, "start of location block"); // Expect opening brace '{'

    std::set<std::string> seen; // Track encountered directives to catch duplicates

    // Parse directives until closing brace
    while (!isAtEnd() && current().type != TokenType::RBRACE) {
        const std::string name = toLower(current().value);
        if (!checkDuplicateDirective(name, seen, kRepeatableLocationDirectives)) {
            throw SyntaxError(formatError("Duplicate directive: '" + name + "'", current().line,
                                          current().column),
                              getContextWindow());
        }
        // Parse and apply a directive to the Location object
        parseLocationDirective(location);
    }

    expect(TokenType::RBRACE, "end of location block"); // Expect closing brace '}'
    return location;
}

void ConfigParser::parseLocationDirective(Location& location) {
    Token key = current(); // Capture the directive token (e.g., "root", "index", etc.)
    advance();             // Consume the directive keyword
    std::vector<std::string> values = collectArgs(kArgTokenTypes);      // Parse directive arguments
    expect(TokenType::SEMICOLON, "semicolon after location directive"); // Ensure `;` terminator
    parseDirective(location, key, values,                               // Dispatch to handler
                   directive::locationHandlers(), key.line, key.column, getContextWindow());
}

////////////////////////
// --- Token Navigation

const Token& ConfigParser::current() const {
    // Return the token at the current parsing position
    return _tokens.at(_pos);
}

const Token& ConfigParser::peek(std::size_t offset) const {
    // Compute the absolute index from current position
    std::size_t index = _pos + offset;
    // Return the token at that index if in bounds, otherwise return the final EOF token
    return (index < _tokens.size()) ? _tokens.at(index) : _tokens.back();
}

const Token& ConfigParser::lookBehind(std::size_t offset) const {
    // Fallback for underflow
    static Token dummy(TokenType::END_OF_FILE, "", 0, 0, 0);
    // Return the token `offset` steps before the current position, or dummy if out of bounds
    return (_pos >= offset) ? _tokens.at(_pos - offset) : dummy;
}

const Token& ConfigParser::advance() {
    if (!isAtEnd())
        ++_pos;       // Move to the next token if not already at the end
    return current(); // Return the new current token
}

bool ConfigParser::isAtEnd() const {
    // True if we've reached the end of the token stream or explicitly hit the EOF token
    return _pos >= _tokens.size() || _tokens[_pos].type == TokenType::END_OF_FILE;
}

bool ConfigParser::match(TokenType type) {
    // If at end or current token doesn't match the expected type, do nothing
    if (isAtEnd() || _tokens[_pos].type != type)
        return false;
    ++_pos;      // Consume the token if it matches
    return true; // Indicate successful match and consumption
}

void ConfigParser::expect(TokenType expected, const std::string& context) {
    // If we're at the end or the current token doesn't match the expected type...
    if (isAtEnd() || _tokens[_pos].type != expected) {
        const Token& actual = isAtEnd() ? _tokens.back() : _tokens[_pos];
        // ...throw an error describing what was expected vs. what was found
        throw UnexpectedToken(formatError("Expected " + debugTokenType(expected) + ", but got " +
                                              debugTokenType(actual.type) + " for " + context,
                                          actual.line, actual.column),
                              getContextWindow());
    }
    ++_pos; // Consume the token if it matched
}

Token ConfigParser::expectOneOf(std::initializer_list<TokenType> types,
                                const std::string&               context) {
    TokenType actual = current().type;

    // Check if the current token matches any of the expected types
    for (TokenType expected : types) {
        if (actual == expected)
            return advance(); // If matched, consume and return it
    }

    // If no match, build an error message listing all expected types
    std::ostringstream msg;
    msg << "Expected ";
    for (auto it = types.begin(); it != types.end(); ++it) {
        if (it != types.begin())
            msg << " or ";
        msg << debugTokenType(*it);
    }
    msg << " for " << context << ", but got " << debugTokenType(actual);

    // Throw a detailed syntax error with contextual highlighting
    throw UnexpectedToken(formatError(msg.str(), current().line, current().column),
                          getContextWindow());
}

std::vector<std::string> ConfigParser::collectArgs(std::initializer_list<TokenType> validTypes) {
    std::vector<std::string> values;

    // Collect tokens while they match one of the allowed types
    while (!isAtEnd()) {
        TokenType t = current().type;

        // Stop if the current token is not in the valid type list
        if (std::find(validTypes.begin(), validTypes.end(), t) == validTypes.end())
            break;

        values.push_back(current().value); // Save the token value
        advance();                         // Consume the token
    }

    return values; // Return the collected argument values
}

int ConfigParser::getLine() const {
    // Return the line number of the current token
    return current().line;
}

/////////////////////
// --- Error Context

std::string ConfigParser::getContextWindow(std::size_t range) const {
    std::ostringstream oss;

    // Calculate the window bounds around the current token position
    std::size_t start = (_pos >= range) ? _pos - range : 0;
    std::size_t end   = std::min(_tokens.size(), _pos + range + 1);

    // Print surrounding tokens with a marker on the current one
    for (std::size_t i = start; i < end; ++i) {
        oss << (i == _pos ? ">> " : "   ") << debugToken(_tokens[i]) << "\n";
    }

    return oss.str(); // Return the formatted multi-line context window
}

std::string ConfigParser::getLineSnippet() const {
    // Extract the full line of source text where the current token is located
    return _tokenizer.extractLine(current().offset);
}
