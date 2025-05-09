/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 15:06:31 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/09 09:08:40 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    ConfigParser.hpp
 * @brief   Defines the ConfigParser class for parsing configuration files.
 *
 * @details This header declares the ConfigParser class, which tokenizes and parses
 *          configuration sources into structured `Config` objects. It supports nested
 *          `server` and `location` blocks, directive dispatch via handler tables,
 *          duplicate detection, and contextual error reporting. It is designed to
 *          provide strong validation and informative error messages during the parsing
 *          process of Webserv configuration files.
 *
 * @ingroup config
 */

#pragma once

#include "config/Config.hpp"
#include "config/parser/ConfigParseError.hpp"
#include "config/tokenizer/Tokenizer.hpp"
#include "config/tokenizer/token.hpp"

#include <set>
#include <string>
#include <vector>

/**
 * @class   ConfigParser
 * @brief   Parses tokenized configuration input into a structured Config object.
 *
 * @details The ConfigParser processes a raw configuration source string by tokenizing
 *          it and building an abstract representation composed of `Server` and `Location`
 *          objects. It handles directive dispatch, argument collection, duplicate detection,
 *          and contextual error reporting. All parsing logic is non-blocking and designed
 *          for robustness and clarity. This parser serves as the main entry point for
 *          configuration ingestion in the Webserv system.
 *
 * @ingroup config
 */
class ConfigParser {
  public:
    //////////////////
    // --- Public API
    /**
     * @brief Constructs a new ConfigParser instance from a configuration source string.
     *
     * @details Initializes the internal tokenizer with the provided raw configuration
     *          source and immediately performs lexical analysis. The resulting tokens
     *          are stored for subsequent parsing into structured objects.
     *
     * @param source The full textual content of the configuration file.
     */
    ConfigParser(std::string source);
    /**
     * @brief Parses the tokenized configuration into a Config object.
     *
     * @details Converts the token stream into a fully structured configuration tree
     *          containing one or more `Server` blocks with nested `Location` blocks.
     *          Performs validation on directive structure, order, and uniqueness.
     *          Throws exceptions if syntax errors or unknown directives are encountered.
     *
     * @return A fully populated Config object representing the parsed configuration.
     *
     * @throws SyntaxError If invalid syntax or unexpected tokens are found.
     * @throws UnexpectedToken If a required token (e.g. `server`, `{`, `;`) is missing.
     */
    Config parseConfig();

  private:
    ////////////////////////////
    // --- Server Block Parsing
    /**
     * @brief Parses a complete `server` block and returns a configured Server object.
     *
     * @details Expects the current token to be the `server` keyword, followed by an opening
     *          brace `{`, then parses all contained directives and optional `location` blocks.
     *          Each directive is validated and dispatched to its handler. Duplicate directives
     *          are rejected unless explicitly marked repeatable.
     *
     * @return A Server instance populated with all parsed configuration values.
     *
     * @throws SyntaxError If the block is malformed or contains duplicate/invalid directives.
     */
    Server parseServer();
    /**
     * @brief Parses and applies a single directive inside a `server` block.
     *
     * @details Consumes a directive keyword, collects its arguments, verifies
     *          the terminating semicolon, and dispatches to the corresponding
     *          handler from the server directive table.
     *
     * @param server The Server object to which the directive will be applied.
     *
     * @throws SyntaxError If the directive is unknown, malformed, or semicolon is missing.
     */
    void parseServerDirective(Server& server);

    //////////////////////////////
    // --- Location Block Parsing
    /**
     * @brief Parses a complete `location` block and returns a configured Location object.
     *
     * @details Expects the `location` keyword followed by a path token and an opening brace.
     *          Parses all valid directives within the block, dispatches them to handlers,
     *          and checks for duplicates unless explicitly allowed.
     *
     * @return A Location instance fully configured with all parsed properties.
     *
     * @throws SyntaxError If the block structure is invalid or contains duplicate/unknown
     * directives.
     */
    Location parseLocation();
    /**
     * @brief Parses and applies a single directive inside a `location` block.
     *
     * @details Consumes a directive token, collects its arguments, checks for a semicolon,
     *          and dispatches the directive to the appropriate handler in the location
     *          directive table.
     *
     * @param location The Location object to which the directive will be applied.
     *
     * @throws SyntaxError If the directive is malformed, unknown, or missing a semicolon.
     */
    void parseLocationDirective(Location& location);

    ////////////////////////
    // --- Token Navigation
    /**
     * @brief Returns the token currently pointed to by the parser.
     *
     * @details This method provides access to the token at the current parse position.
     *          It is used as the primary lookahead mechanism during parsing.
     *
     * @return A const reference to the current Token.
     */
    const Token& current() const;
    /**
     * @brief Peeks ahead at a token relative to the current position.
     *
     * @details Returns the token at `_pos + offset` without advancing the parser.
     *          If the requested index is out of bounds, the final EOF token is returned.
     *
     * @param offset Number of tokens to look ahead (default is 1).
     * @return A const reference to the future Token.
     */
    const Token& peek(std::size_t offset = 1) const;
    /**
     * @brief Returns a token before the current position.
     *
     * @details Used for contextual inspection during error reporting or validation.
     *          If the requested offset goes before the start of the token stream,
     *          a static dummy EOF token is returned.
     *
     * @param offset Number of tokens to look behind (default is 1).
     * @return A const reference to the previous Token or a fallback EOF token.
     */
    const Token& lookBehind(std::size_t offset = 1) const;
    /**
     * @brief Advances the parser to the next token.
     *
     * @details Increments the internal cursor if not already at the end of the token stream.
     *          Returns the new current token after advancing.
     *
     * @return A const reference to the token now pointed to by the parser.
     */
    const Token& advance();
    /**
     * @brief Checks whether the parser has reached the end of the token stream.
     *
     * @details Returns true if the current position is at or beyond the final token,
     *          or if the current token is the special `END_OF_FILE` marker.
     *
     * @return `true` if parsing is complete, otherwise `false`.
     */
    bool isAtEnd() const;
    /**
     * @brief Conditionally consumes a token if it matches the given type.
     *
     * @details If the current token matches the expected type, advances the parser and
     *          returns `true`. Otherwise, the parser remains at the same position and
     *          returns `false`.
     *
     * @param type The expected TokenType to match.
     * @return `true` if the token was matched and consumed, otherwise `false`.
     */
    bool match(TokenType type);
    /**
     * @brief Consumes a token of the expected type or throws an error.
     *
     * @details Verifies that the current token matches the expected type.
     *          If it does, the parser advances. Otherwise, throws an
     *          `UnexpectedToken` with contextual error information.
     *
     * @param expected The expected token type.
     * @param context  A short description of the expected construct (used in the error message).
     *
     * @throws UnexpectedToken If the token does not match the expected type.
     */
    void expect(TokenType expected, const std::string& context);
    /**
     * @brief Expects one of several token types and consumes it if matched.
     *
     * @details Checks whether the current token matches any of the specified types.
     *          If a match is found, the token is consumed and returned. Otherwise,
     *          an `UnexpectedToken` exception is thrown with detailed context.
     *
     * @param types   A list of acceptable token types.
     * @param context A short description of what the token represents (for error reporting).
     * @return The matched and consumed Token.
     *
     * @throws UnexpectedToken If none of the expected types match the current token.
     */
    Token expectOneOf(std::initializer_list<TokenType> types, const std::string& context);
    /**
     * @brief Collects a sequence of valid argument tokens.
     *
     * @details Consumes and collects consecutive tokens whose types match
     *          any in the provided list. Parsing stops at the first token
     *          that is not in the allowed set.
     *
     * @param validTypes List of token types considered valid arguments.
     * @return A vector of argument values as strings.
     */
    std::vector<std::string> collectArgs(std::initializer_list<TokenType> validTypes);

    /////////////////////
    // --- Error Context
    /**
     * @brief Returns the line number of the current token.
     *
     * @details This is typically used for diagnostics and error reporting
     *          to indicate where in the configuration the parser is operating.
     *
     * @return The 1-based line number of the current token.
     */
    int getLine() const;
    /**
     * @brief Returns a window of tokens surrounding the current parsing position.
     *
     * @details Provides a visual context around the current token by showing a range
     *          of tokens before and after it. The current token is marked with `>>`.
     *          Used primarily in error messages to improve debuggability.
     *
     * @param range Number of tokens to show before and after the current one (default is 2).
     * @return A formatted string showing the context window.
     */
    std::string getContextWindow(std::size_t range = 2) const;
    /**
     * @brief Extracts the original source line containing the current token.
     *
     * @details Returns the full line of configuration text where the current token appears.
     *          This is useful for generating human-readable error messages with exact line content.
     *
     * @return The source line as a string.
     */
    std::string getLineSnippet() const;

  private:
    Tokenizer          _tokenizer; ///< Tokenizer instance used to produce the token stream.
    std::vector<Token> _tokens;    ///< Flattened list of tokens extracted from the source input.
    std::size_t        _pos = 0;   ///< Current index in the token stream.
};
