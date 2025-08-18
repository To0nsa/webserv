/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/18 12:18:00 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 12:29:57 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    ConfigParser.cpp
 * @brief   Implements parsing of servers, locations, and directives.
 *
 * @details Walks the token stream, enforces grammar, and dispatches each
 *          directive to handler maps to populate @ref Server and @ref Location.
 *          Provides precise diagnostics with contextual line snippets.
 *
 * @ingroup config_parsing
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

/**
 * @brief Directives allowed multiple times inside a server block.
 * @ingroup config_parsing
 */
static const std::unordered_set<std::string> kRepeatableServerDirectives = {"error_page"};

/**
 * @brief Directives allowed multiple times inside a location block.
 * @ingroup config_parsing
 */
static const std::unordered_set<std::string> kRepeatableLocationDirectives = {
    "methods",
    "cgi_interpreter",
};

/**
 * @brief Token types accepted as directive arguments (ordered by frequency).
 * @ingroup config_parsing
 */
static constexpr std::array<TokenType, 3> kArgTokenTypes = {
    TokenType::STRING,
    TokenType::NUMBER,
    TokenType::IDENTIFIER,
};

/**
 * @brief Checks whether a directive name is allowed (not duplicated).
 *
 * @details Tracks encountered directive names within a block (`server` or `location`).
 *          If the directive is marked as repeatable, it is always accepted.
 *          Otherwise, the name is inserted into the `seen` set:
 *          - If insertion succeeds, the directive is new and valid.
 *          - If insertion fails, the directive is a duplicate and rejected.
 *
 * @param name       The directive keyword being checked.
 * @param seen       Set of already encountered directive names in the current block.
 * @param repeatable Set of directive names allowed to appear multiple times.
 *
 * @return `true` if the directive is valid (new or repeatable),
 *         `false` if it is a duplicate and not allowed.
 *
 * @ingroup config_parser
 */
bool checkDuplicateDirective(const std::string& name, std::unordered_set<std::string>& seen,
                             const std::unordered_set<std::string>& repeatable) {
    return repeatable.contains(name) || seen.insert(name).second;
}

/**
 * @brief Dispatches a configuration directive to its registered handler.
 *
 * @details Looks up the directive keyword (from the current token) in the
 *          provided handler table. If a matching handler is found, it is
 *          invoked with the target object (Server or Location), parsed values,
 *          source position (line/column), and contextual snippet. If no handler
 *          exists for the directive name, a @ref SyntaxError is thrown.
 *
 * @tparam T          The type of the configuration target (e.g., `Server` or `Location`).
 * @tparam HandlerMap A mapping from directive name (`std::string`) to a handler
 *                    function/lambda capable of applying the directive.
 *
 * @param target  The configuration object being populated (Server/Location).
 * @param token   The token representing the directive keyword.
 * @param values  List of string arguments parsed for the directive.
 * @param handlers Map of directive names to handler functions.
 * @param line    Line number in the source config where the directive occurs.
 * @param column  Column number in the source config where the directive occurs.
 * @param ctx     A snippet of the source line for contextual error messages.
 *
 * @throws SyntaxError If the directive name is unknown or unhandled.
 *
 * @ingroup config_parser
 */
template <typename T, typename HandlerMap>
void parseDirective(T& target, const Token& token, std::vector<std::string>& values,
                    const HandlerMap& handlers, int line, int column, const std::string& ctx) {
    const std::string& directiveName = token.value;

    // Look up the directive in the handler table
    auto handlerIt = handlers.find(directiveName);
    if (handlerIt == handlers.end()) {
        throw SyntaxError(formatError("Unknown directive: '" + token.value + "'", line, column),
                          ctx);
    }

    // Dispatch to the registered handler function
    auto& handler = handlerIt->second;
    handler(target, values, line, column, ctx);
}

} // namespace

//=== Public API ==========================================================

/**
 * @brief Constructs a ConfigParser instance from raw configuration text.
 *
 * @details Initializes the internal tokenizer with the provided configuration
 *          source string. Immediately tokenizes the input and stores the resulting
 *          token sequence for parsing. This makes subsequent parsing methods
 *          (`parseConfig`, `parseServer`, etc.) operate directly on a prebuilt
 *          token stream rather than raw text.
 *
 * @param source Raw configuration string to be parsed.
 *
 * @ingroup config_parser
 */
ConfigParser::ConfigParser(std::string source) : _tokenizer(std::move(source)) {
    _tokens = _tokenizer.tokenize();
}

/**
 * @brief Parses the full configuration input into a Config object.
 *
 * @details Expects the top-level configuration to consist of one or more
 *          `server` blocks. Ensures that the input is non-empty, validates
 *          the presence of `server` keywords, and parses each block in turn.
 *          If invalid structure or unexpected tokens are encountered, a
 *          `SyntaxError` is thrown with contextual information.
 *
 * @return A fully constructed Config object containing all parsed servers.
 *
 * @throws SyntaxError If the configuration is empty, missing `server` blocks,
 *                     or contains unexpected tokens after a block.
 *
 * @ingroup config_parser
 */
Config ConfigParser::parseConfig() {
    Config config;

    // Reject completely empty configuration
    if (isAtEnd()) {
        throw SyntaxError(formatError("Empty configuration", 1, 1), getLineSnippet());
    }

    // Parse one or more server blocks
    while (!isAtEnd()) {
        // Each top-level block must start with the 'server' keyword
        if (current().type != TokenType::KEYWORD_SERVER) {
            throw SyntaxError(
                formatError("Expected 'server' block", current().line, current().column),
                getLineSnippet());
        }

        // Parse the full server block and add it to the Config
        config.addServer(parseServer());

        // After a server block, only EOF or another 'server' is allowed
        if (!isAtEnd() && current().type != TokenType::KEYWORD_SERVER &&
            current().type != TokenType::END_OF_FILE) {
            throw SyntaxError(formatError("Unexpected token after server block", current().line,
                                          current().column),
                              getLineSnippet());
        }
    }

    return config;
}

//=== Server Block Parsing ===============================================

/**
 * @brief Parses a single `server` block into a Server object.
 *
 * @details Ensures the block starts with the `server` keyword and opening brace,
 *          then iterates through its directives and nested `location` blocks.
 *          Each directive is validated against duplicates (unless repeatable)
 *          and dispatched to the appropriate handler.
 *
 * @return A fully populated Server object representing the parsed block.
 *
 * @throws SyntaxError If the block is malformed, contains duplicate directives,
 *                     or is missing the expected closing brace.
 *
 * @ingroup config_parser
 */
Server ConfigParser::parseServer() {
    // Validate required "server {" start sequence
    expect(TokenType::KEYWORD_SERVER, "server block");
    expect(TokenType::LBRACE, "start of server block");

    Server                          server; // New Server instance
    std::unordered_set<std::string> seen;   // Track seen directives (for duplicates)

    // Loop until closing brace or EOF
    while (!isAtEnd() && current().type != TokenType::RBRACE) {
        if (current().type == TokenType::KEYWORD_LOCATION) {
            // Nested location block -> parse recursively
            server.addLocation(parseLocation());
        } else {
            // Check for duplicate directives unless repeatable
            const std::string name = current().value;
            if (!checkDuplicateDirective(name, seen, kRepeatableServerDirectives)) {
                throw SyntaxError(formatError("Duplicate directive: '" + name + "'", current().line,
                                              current().column),
                                  getLineSnippet());
            }
            // Parse directive and apply it to the server
            parseServerDirective(server);
        }
    }

    // Ensure proper block termination
    expect(TokenType::RBRACE, "end of server block");
    return server;
}

/**
 * @brief Parses and applies a single directive inside a `server` block.
 *
 * @details Reads the current directive token (e.g., `listen`, `host`, etc.),
 *          collects its arguments, enforces a terminating semicolon, and then
 *          dispatches the directive to the appropriate handler function from
 *          the server handler table.
 *
 * @param server Reference to the Server object being populated.
 *
 * @throws SyntaxError If the directive is malformed, missing arguments, or
 *                     missing its terminating semicolon.
 *
 * @ingroup config_parser
 */
void ConfigParser::parseServerDirective(Server& server) {
    Token key = current(); // Capture the directive keyword token
    advance();             // Consume the keyword (move to its arguments)

    // Collect directive arguments (strings, numbers, or identifiers)
    std::vector<std::string> values = collectArgs(kArgTokenTypes);

    // Ensure directive ends with a semicolon
    expect(TokenType::SEMICOLON, "semicolon after server directive");

    // Dispatch directive to its handler, applying it to the server
    parseDirective(server, key, values, directive::serverHandlers(), key.line, key.column,
                   getLineSnippet());
}

//=== Location Block Parsing =============================================

/**
 * @brief Parses a single `location` block inside a `server` block.
 *
 * @details Ensures the block starts with the `location` keyword, validates and
 *          sets the location path, then iterates over its contained directives.
 *          Each directive is validated for duplicates (unless repeatable) and
 *          dispatched to the appropriate handler. The block must end with a
 *          closing brace `}`.
 *
 * @return A fully constructed `Location` object populated with directives.
 *
 * @throws SyntaxError If the block is malformed, the path is missing or invalid,
 *                     contains duplicate directives, or is missing required
 *                     braces.
 *
 * @ingroup config_parser
 */
Location ConfigParser::parseLocation() {
    expect(TokenType::KEYWORD_LOCATION, "location block"); // Must start with `location`

    Location location;

    // Validate that the next token is a valid path (string or identifier)
    TokenType type = current().type;
    if (type != TokenType::STRING && type != TokenType::IDENTIFIER) {
        throw SyntaxError(formatError("Expected location path after 'location', but got '" +
                                          current().value + "'",
                                      current().line, current().column),
                          getLineSnippet());
    }

    // Extract and validate the path token
    Token pathTok = current();
    if (pathTok.value.empty() || pathTok.value[0] != '/') {
        throw SyntaxError(
            formatError("Location path must start with '/' (got '" + pathTok.value + "')",
                        pathTok.line, pathTok.column),
            getLineSnippet());
    }
    location.setPath(pathTok.value);
    advance(); // Consume the path token

    expect(TokenType::LBRACE, "start of location block"); // Require opening `{`

    std::unordered_set<std::string> seen; // Track encountered directives to catch duplicates

    // Parse directives until closing brace `}`
    while (!isAtEnd() && current().type != TokenType::RBRACE) {
        const std::string name = current().value;
        if (!checkDuplicateDirective(name, seen, kRepeatableLocationDirectives)) {
            throw SyntaxError(formatError("Duplicate directive: '" + name + "'", current().line,
                                          current().column),
                              getLineSnippet());
        }
        parseLocationDirective(location); // Apply directive to the location
    }

    expect(TokenType::RBRACE, "end of location block"); // Ensure closing `}`
    return location;
}

/**
 * @brief Parses a single directive inside a `location` block.
 *
 * @details Reads the directive keyword, collects its arguments, enforces that
 *          it is terminated by a semicolon `;`, and dispatches it to the
 *          appropriate handler function via the directive table.
 *
 * @param location Reference to the `Location` object where parsed directive
 *                 data will be applied.
 *
 * @throws SyntaxError If arguments are invalid, missing, or the directive
 *                     is not followed by a semicolon.
 *
 * @ingroup config_parser
 */
void ConfigParser::parseLocationDirective(Location& location) {
    Token key = current(); // Capture directive keyword token (e.g., "root", "index", etc.)
    advance();             // Consume the directive keyword

    // Collect arguments that follow the directive
    std::vector<std::string> values = collectArgs(kArgTokenTypes);

    // Ensure the directive is properly terminated by a semicolon
    expect(TokenType::SEMICOLON, "semicolon after location directive");

    // Dispatch the directive to the correct handler function from the table
    parseDirective(location, key, values, directive::locationHandlers(), key.line, key.column,
                   getLineSnippet());
}

//=== Token Navigation ====================================================

/**
 * @brief Returns the current token (bounds-checked).
 * @ingroup config_parsing
 */
const Token& ConfigParser::current() const {
    return _tokens.at(_pos);
}

/**
 * @brief Returns the previous token or the first token if at start.
 * @ingroup config_parsing
 */
const Token& ConfigParser::previous() const {
    if (_pos == 0) {
        return _tokens.at(0);
    }
    return _tokens.at(_pos - 1);
}

/**
 * @brief Peeks a token at an offset ≥ 1 from current.
 *
 * @param offset Relative index (defaults to 1).
 * @return Token at index or EOF token if out of range.
 * @ingroup config_parsing
 */
const Token& ConfigParser::peek(std::size_t offset) const {
    std::size_t index = _pos + offset;
    if (index < _tokens.size()) {
        return _tokens.at(index);
    }
    return _tokens.back();
}

/**
 * @brief Looks behind by a given offset, or returns an EOF dummy.
 *
 * @param offset Distance to look back (defaults to 1).
 * @ingroup config_parsing
 */
const Token& ConfigParser::lookBehind(std::size_t offset) const {
    static Token dummy(TokenType::END_OF_FILE, "", 0, 0, 0);

    if (_pos >= offset) {
        return _tokens.at(_pos - offset);
    }
    return dummy;
}

/**
 * @brief Advances to the next token and returns it.
 *
 * @return New current token after increment (or EOF if at end).
 * @ingroup config_parsing
 */
const Token& ConfigParser::advance() {
    if (!isAtEnd())
        ++_pos;
    return current();
}

/**
 * @brief Checks if the parser has consumed all tokens or reached EOF.
 *
 * @return `true` if no more tokens are available.
 * @ingroup config_parsing
 */
bool ConfigParser::isAtEnd() const {
    return _pos >= _tokens.size() || _tokens[_pos].type == TokenType::END_OF_FILE;
}

/**
 * @brief Consumes the current token if its type matches.
 *
 * @param type Expected token type.
 * @return `true` if matched/consumed.
 * @ingroup config_parsing
 */
bool ConfigParser::match(TokenType type) {
    if (isAtEnd() || _tokens[_pos].type != type)
        return false;
    ++_pos;
    return true;
}

/**
 * @brief Ensures that the current token matches a specific expected type.
 *
 * @details This method validates that the parser is currently positioned
 *          at a token of type @p expected. If the token matches, it is consumed
 *          (the parser advances by one). If not, an `UnexpectedToken` error is thrown
 *          with details about what was expected versus what was found.
 *
 * @param expected The exact token type required at this position.
 * @param context  Human-readable context for error reporting
 *                 (unused here but provided for consistency).
 *
 * @throws UnexpectedToken If the current token does not match @p expected.
 *
 * @ingroup config_parser
 */
void ConfigParser::expect(TokenType expected, const std::string& context) {
    (void) context; // Currently unused, but kept for interface consistency

    // If at end or wrong type → prepare error
    if (isAtEnd() || _tokens[_pos].type != expected) {
        const Token* actual;

        if (isAtEnd()) {
            actual = &_tokens.back(); // fallback to EOF token
        } else {
            actual = &_tokens[_pos]; // offending token
        }

        // Build and throw detailed error
        throw UnexpectedToken(formatError("Expected " + debugTokenType(expected) + ", but got " +
                                              debugTokenType(actual->type),
                                          actual->line, actual->column),
                              getLineSnippet());
    }

    ++_pos; // Consume token if matched
}

/**
 * @brief Ensures the current token matches one of several expected types.
 *
 * @details This method is used when a directive or grammar rule allows
 *          multiple token types in the same position (e.g., `STRING` or `IDENTIFIER`).
 *          It checks if the current token matches any type in @p types:
 *
 *          - If a match is found, the token is consumed via `advance()` and returned.
 *          - If none match, an `UnexpectedToken` exception is thrown with a
 *            descriptive error message listing all expected types and the actual one.
 *
 * @param types   List of acceptable token types (initializer list).
 * @param context Human-readable context for error reporting.
 *
 * @return The matched token, already consumed from the stream.
 *
 * @throws UnexpectedToken If the current token does not match any expected type.
 *
 * @ingroup config_parser
 */
Token ConfigParser::expectOneOf(std::initializer_list<TokenType> types,
                                const std::string&               context) {
    TokenType actual = current().type; // Current token type

    // Try matching against the expected set
    for (TokenType expected : types) {
        if (actual == expected) {
            return advance(); // Consume & return if found
        }
    }

    // Build detailed error message if no match
    std::ostringstream msg;
    msg << "Expected ";
    for (auto it = types.begin(); it != types.end(); ++it) {
        if (it != types.begin()) {
            msg << " or "; // Separate multiple expected types
        }
        msg << debugTokenType(*it); // Append readable token type
    }
    msg << " for " << context << ", but got " << debugTokenType(actual);

    // Throw with contextual line snippet
    throw UnexpectedToken(formatError(msg.str(), current().line, current().column),
                          getLineSnippet());
}

/**
 * @brief Collects a list of argument values following a directive.
 *
 * @details This function consumes tokens that belong to the directive's argument list.
 *          It accepts only tokens of specific types (`STRING`, `NUMBER`, `IDENTIFIER`),
 *          provided in the @p validTypes span. Arguments may be separated by commas.
 *          Example:
 *          ```
 *          root /var/www;
 *          methods GET, POST, DELETE;
 *          ```
 *
 *          - Stops parsing when a token does not match a valid argument type.
 *          - Supports comma-separated lists of arguments.
 *          - Throws if a comma is not followed by a valid argument.
 *
 * @param validTypes A span of allowed token types for directive arguments.
 *
 * @return A vector of string values representing the collected arguments.
 *
 * @throws SyntaxError If a comma is not followed by a valid argument type.
 *
 * @ingroup config_parser
 */
std::vector<std::string> ConfigParser::collectArgs(std::span<const TokenType> validTypes) {
    std::vector<std::string> values; // Accumulate argument values here

    // Continue as long as we are not at end-of-file
    while (!isAtEnd()) {
        TokenType t = current().type;

        // If current token type is not among the allowed ones, stop collecting
        if (std::find(validTypes.begin(), validTypes.end(), t) == validTypes.end())
            break;

        // Append the argument value (string/number/identifier)
        values.push_back(current().value);
        advance(); // Move to the next token

        // Handle optional comma-separated arguments
        while (match(TokenType::COMMA)) {
            // A comma must be followed by another valid argument
            if (isAtEnd() || std::find(validTypes.begin(), validTypes.end(), current().type) ==
                                 validTypes.end()) {
                throw SyntaxError(
                    formatError("Expected value after comma", current().line, current().column),
                    getLineSnippet());
            }

            // Append the value after the comma
            values.push_back(current().value);
            advance(); // Consume argument token
        }
    }

    return values; // Return the collected argument list
}

//=== Error Context =======================================================

/**
 * @brief Extracts the full source line of the last consumed token.
 *
 * @return Source line for diagnostic display.
 * @ingroup config_parsing
 */
std::string ConfigParser::getLineSnippet() const {
    return _tokenizer.extractLine(previous().offset);
}
