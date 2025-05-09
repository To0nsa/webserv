/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   token.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 00:55:06 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/08 16:54:59 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    token.hpp
 * @brief   Token representation for the configuration parser.
 *
 * @details
 * Declares the `Token` struct, `TokenType` enum, and related
 * helpers used during lexical analysis of the configuration file.
 * Tokens are produced by the `Tokenizer` and consumed by the `ConfigParser`.
 *
 * @ingroup config
 */

#pragma once

#include <string>

/**
 * @enum   TokenType
 * @brief  Enumeration of all recognized token types during config parsing.
 *
 * @details
 * Represents both syntactic tokens (braces, semicolons, etc.) and
 * semantic tokens such as identifiers, literals, and configuration keywords.
 * These types are used by the tokenizer and parser to validate structure and semantics.
 *
 * @ingroup config
 */
enum class TokenType {
    IDENTIFIER,  ///< A generic identifier (e.g., directive name or argument)
    NUMBER,      ///< A numeric literal (may include optional size suffix)
    STRING,      ///< A quoted string literal
    LBRACE,      ///< `{` — begins a block
    RBRACE,      ///< `}` — ends a block
    SEMICOLON,   ///< `;` — terminates a directive
    END_OF_FILE, ///< Special token marking the end of the input

    // ───── Keywords ─────

    KEYWORD_SERVER,               ///< `server` block keyword
    KEYWORD_LOCATION,             ///< `location` block keyword
    KEYWORD_LISTEN,               ///< `listen` directive
    KEYWORD_HOST,                 ///< `host` directive
    KEYWORD_ROOT,                 ///< `root` directive
    KEYWORD_INDEX,                ///< `index` directive
    KEYWORD_AUTOINDEX,            ///< `autoindex` directive
    KEYWORD_METHODS,              ///< `methods` directive
    KEYWORD_UPLOAD_STORE,         ///< `upload_store` directive
    KEYWORD_RETURN,               ///< `return` directive (for redirection)
    KEYWORD_ERROR_PAGE,           ///< `error_page` directive
    KEYWORD_CLIENT_MAX_BODY_SIZE, ///< `client_max_body_size` directive
    KEYWORD_CGI_EXTENSION         ///< `cgi_extension` directive
};

/**
 * @struct Token
 * @brief Represents a single token extracted from the configuration input.
 *
 * @details
 * Stores information about the token type, its textual value, and its source location
 * (line and column) within the configuration file. This struct is used during the
 * lexical analysis phase for syntax validation and to generate meaningful error messages.
 *
 * @ingroup config
 */
struct Token {
    TokenType   type;   ///< Type of the token (identifier, keyword, etc.)
    std::string value;  ///< Lexical string value of the token
    int         line;   ///< Line number where the token begins
    int         column; ///< Column offset (zero-based)
    std::size_t offset; ///< Byte offset in the original input string

    Token() = default;
    /**
     * @brief Constructs a new Token with specified attributes.
     *
     * @details This constructor initializes a Token with its type, value, source location
     *          (line and column), and the byte offset in the input string. It is used during
     *          lexical analysis to represent a specific piece of input for further processing.
     *
     * @param t The type of the token (e.g., `IDENTIFIER`, `NUMBER`, etc.).
     * @param v The string value of the token.
     * @param l The line number where the token was found in the configuration file.
     * @param c The column number where the token was found in the configuration file.
     * @param o The byte offset in the input string where the token starts.
     * @ingroup config
     */
    Token(TokenType t, const std::string& v, int l, int c, std::size_t o);
    ~Token()                           = default;
    Token(const Token&)                = default;
    Token& operator=(const Token&)     = default;
    Token(Token&&) noexcept            = default;
    Token& operator=(Token&&) noexcept = default;
};

/**
 * @brief Returns a string representation of a `TokenType`.
 *
 * @details This function converts a `TokenType` enum value into a human-readable
 *          string for debugging purposes. It is primarily used for logging and
 *          error reporting during the tokenization and parsing processes.
 *
 * @param type The `TokenType` enum value to convert.
 * @return A string representation of the `TokenType` (e.g., "IDENTIFIER", "STRING").
 * @ingroup config
 */
std::string debugTokenType(TokenType type);

/**
 * @brief Returns a string representation of a `Token` for debugging.
 *
 * @details This function generates a human-readable string that includes the
 *          token's type, its value, and its source location (line and column).
 *          It is useful for logging and diagnosing parsing issues, providing
 *          a full view of the token's context.
 *
 * @param token The `Token` to represent as a string.
 * @return A string representation of the `Token`, including type, value, line, and column.
 * @ingroup config
 */
std::string debugToken(const Token& token);

/** @} */ // end of Tokenizer
