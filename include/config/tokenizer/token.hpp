/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   token.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 00:55:06 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/20 23:50:46 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <string>

enum class TokenType {
    IDENTIFIER,  ///< A generic identifier (directive name or argument)
    NUMBER,      ///< A numeric literal (may include optional size suffix)
    STRING,      ///< A quoted string literal
    LBRACE,      ///< `{` — begins a block
    RBRACE,      ///< `}` — ends a block
    SEMICOLON,   ///< `;` — terminates a directive
    COMMA,       ///< `,` — separates arguments
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

struct Token {
    TokenType   type;   ///< Type of the token (identifier, keyword, etc.)
    std::string value;  ///< Lexical string value of the token
    int         line;   ///< Line number where the token begins
    int         column; ///< Column offset (zero-based)
    std::size_t offset; ///< Byte offset in the original input string

    Token(TokenType t, const std::string& v, int l, int c, std::size_t o);
};

std::string debugTokenType(TokenType type);
std::string debugToken(const Token& token);
