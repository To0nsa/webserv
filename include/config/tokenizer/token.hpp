/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   token.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 00:55:06 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 11:58:30 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    token.hpp
 * @brief   Token and TokenType declarations for the config lexer.
 *
 * @details Defines the token enum and POD struct used by the Tokenizer to
 *          represent lexemes with their source location (line/column/offset).
 *
 * @ingroup config_tokenizing
 */

#pragma once

#include <string>

/**
 * @brief Token categories recognized by the configuration lexer.
 *
 * @ingroup config_tokenizing
 */
enum class TokenType {
    IDENTIFIER,  ///< A generic identifier (directive name or argument)
    NUMBER,      ///< A numeric literal (may include optional size suffix)
    STRING,      ///< A quoted string literal
    LBRACE,      ///< `{` — begins a block
    RBRACE,      ///< `}` — ends a block
    SEMICOLON,   ///< `;` — terminates a directive
    COMMA,       ///< `,` — separates arguments
    END_OF_FILE, ///< Special token marking the end of the input

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
 * @brief Plain-old-data token with source coordinates.
 *
 * @details Carries the token type, lexical value, and its position within the
 *          original input (line, column, byte offset) for diagnostics.
 *
 * @ingroup config_tokenizing
 */
struct Token {
    TokenType   type;   ///< Type of the token (identifier, keyword, etc.)
    std::string value;  ///< Lexical string value of the token
    int         line;   ///< Line number where the token begins (1-based)
    int         column; ///< Column number where the token begins (1-based)
    std::size_t offset; ///< Byte offset in the original input string

    Token(TokenType t, const std::string& v, int l, int c, std::size_t o);
};

std::string debugTokenType(TokenType type);
std::string debugToken(const Token& token);
