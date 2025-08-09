/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/09 08:46:22 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/09 12:11:52 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/parser/ConfigParser.hpp"
#include "config/Config.hpp"
#include "config/parser/ConfigParseError.hpp"
#include "config/parser/directive_handler_table.hpp"
#include "config/tokenizer/Tokenizer.hpp"
#include "core/Location.hpp"
#include "core/Server.hpp"
#include "utils/errorUtils.hpp"
#include "utils/stringUtils.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <functional>
#include <memory>
#include <span>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

static const std::unordered_set<std::string> kRepeatableServerDirectives   = {"error_page"};
static const std::unordered_set<std::string> kRepeatableLocationDirectives = {"methods",
                                                                              "cgi_interpreter"};
static constexpr std::array<TokenType, 3>    kArgTokenTypes = {TokenType::STRING, TokenType::NUMBER,
                                                               TokenType::IDENTIFIER};

bool checkDuplicateDirective(const std::string& name, std::unordered_set<std::string>& seen,
                             const std::unordered_set<std::string>& repeatable) {
    return repeatable.contains(name) || seen.insert(name).second;
}

template <typename T, typename HandlerMap>
void parseDirective(T& target, const Token& token, std::vector<std::string>& values,
                    const HandlerMap& handlers, int line, int column, const std::string& ctx) {
    const std::string& directiveName = token.value;

    auto handlerIt = handlers.find(directiveName);
    if (handlerIt == handlers.end()) {
        throw SyntaxError(formatError("Unknown directive: '" + token.value + "'", line, column),
                          ctx);
    }

    auto& handler = handlerIt->second;
    handler(target, values, line, column, ctx);
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
        throw SyntaxError(formatError("Empty configuration", 1, 1), getLineSnippet());
    }

    // Top-level config must consist of one or more `server` blocks
    while (!isAtEnd()) {
        // If the current token is not `server`, fail early
        if (current().type != TokenType::KEYWORD_SERVER) {
            throw SyntaxError(
                formatError("Expected 'server' block", current().line, current().column),
                getLineSnippet());
        }

        // Parse a full server block and append it to the config
        config.addServer(parseServer());

        // After a server block, only EOF or another server block is allowed
        if (!isAtEnd() && current().type != TokenType::KEYWORD_SERVER &&
            current().type != TokenType::END_OF_FILE) {
            throw SyntaxError(formatError("Unexpected token after server block", current().line,
                                          current().column),
                              getLineSnippet());
        }
    }

    return config;
}

////////////////////////////
// --- Server Block Parsing

Server ConfigParser::parseServer() {
    expect(TokenType::KEYWORD_SERVER, "server block");  // Ensure block starts with 'server'
    expect(TokenType::LBRACE, "start of server block"); // Expect opening brace '{'

    Server                          server;
    std::unordered_set<std::string> seen; // Track directives to detect duplicates

    // Loop until closing '}' or end of file
    while (!isAtEnd() && current().type != TokenType::RBRACE) {
        if (current().type == TokenType::KEYWORD_LOCATION) {
            // Parse and attach a location block to the server
            server.addLocation(parseLocation());
        } else {
            // Normalize directive name to lowercase and check for duplicates
            const std::string name = current().value;
            if (!checkDuplicateDirective(name, seen, kRepeatableServerDirectives)) {
                throw SyntaxError(formatError("Duplicate directive: '" + name + "'", current().line,
                                              current().column),
                                  getLineSnippet());
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
                   key.line, key.column, getLineSnippet());
}

//////////////////////////////
// --- Location Block Parsing

Location ConfigParser::parseLocation() {
    expect(TokenType::KEYWORD_LOCATION, "location block"); // Ensure block starts with 'location'

    Location location;

    // Validate path token exists and is of correct type
    TokenType type = current().type;
    if (type != TokenType::STRING && type != TokenType::IDENTIFIER) {
        throw SyntaxError(formatError("Expected location path after 'location', but got '" +
                                          current().value + "'",
                                      current().line, current().column),
                          getLineSnippet());
    }

    Token pathTok = current();
    if (pathTok.value.empty() || pathTok.value[0] != '/') {
        throw SyntaxError(
            formatError("Location path must start with '/' (got '" + pathTok.value + "')",
                        pathTok.line, pathTok.column),
            getLineSnippet());
    }
    location.setPath(pathTok.value);
    advance(); // consume the path token

    expect(TokenType::LBRACE, "start of location block"); // Expect opening brace '{'

    std::unordered_set<std::string> seen; // Track encountered directives to catch duplicates

    // Parse directives until closing brace
    while (!isAtEnd() && current().type != TokenType::RBRACE) {
        const std::string name = current().value;
        if (!checkDuplicateDirective(name, seen, kRepeatableLocationDirectives)) {
            throw SyntaxError(formatError("Duplicate directive: '" + name + "'", current().line,
                                          current().column),
                              getLineSnippet());
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
                   directive::locationHandlers(), key.line, key.column, getLineSnippet());
}

////////////////////////
// --- Token Navigation

const Token& ConfigParser::current() const {
    // Return the token at the current parsing position
    return _tokens.at(_pos);
}

const Token& ConfigParser::previous() const {
    if (_pos == 0) {
        return _tokens.at(0); // fallback to first token
    }
    return _tokens.at(_pos - 1);
}

const Token& ConfigParser::peek(std::size_t offset) const {
    std::size_t index = _pos + offset;
    if (index < _tokens.size()) {
        return _tokens.at(index);
    }
    return _tokens.back(); // Fallback to EOF token
}

const Token& ConfigParser::lookBehind(std::size_t offset) const {
    static Token dummy(TokenType::END_OF_FILE, "", 0, 0, 0);

    if (_pos >= offset) {
        return _tokens.at(_pos - offset);
    }
    return dummy;
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
    (void) context;

    if (isAtEnd() || _tokens[_pos].type != expected) {
        const Token* actual;

        if (isAtEnd()) {
            actual = &_tokens.back(); // fallback to last token (should be EOF)
        } else {
            actual = &_tokens[_pos];
        }

        throw UnexpectedToken(formatError("Expected " + debugTokenType(expected) + ", but got " +
                                              debugTokenType(actual->type),
                                          actual->line, actual->column),
                              getLineSnippet());
    }

    ++_pos; // Consume the token if it matched
}

Token ConfigParser::expectOneOf(std::initializer_list<TokenType> types,
                                const std::string&               context) {
    TokenType actual = current().type;

    // Check if the current token matches any of the expected types
    for (TokenType expected : types) {
        if (actual == expected) {
            return advance(); // If matched, consume and return it
        }
    }

    // If no match, build an error message listing all expected types
    std::ostringstream msg;
    msg << "Expected ";
    for (auto it = types.begin(); it != types.end(); ++it) {
        if (it != types.begin()) {
            msg << " or ";
        }
        msg << debugTokenType(*it);
    }
    msg << " for " << context << ", but got " << debugTokenType(actual);

    // Throw a detailed syntax error with contextual highlighting
    throw UnexpectedToken(formatError(msg.str(), current().line, current().column),
                          getLineSnippet());
}

std::vector<std::string> ConfigParser::collectArgs(std::span<const TokenType> validTypes) {
    std::vector<std::string> values;

    while (!isAtEnd()) {
        TokenType t = current().type;

        if (std::find(validTypes.begin(), validTypes.end(), t) == validTypes.end())
            break;

        values.push_back(current().value);
        advance();

        while (match(TokenType::COMMA)) {
            if (isAtEnd() || std::find(validTypes.begin(), validTypes.end(), current().type) ==
                                 validTypes.end()) {
                throw SyntaxError(
                    formatError("Expected value after comma", current().line, current().column),
                    getLineSnippet());
            }
            values.push_back(current().value);
            advance();
        }
    }

    return values;
}

/////////////////////
// --- Error Context

std::string ConfigParser::getLineSnippet() const {
    // Extract the full line of source text where the previous token is located
    return _tokenizer.extractLine(previous().offset);
}
