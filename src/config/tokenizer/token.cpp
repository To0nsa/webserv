/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   token.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/18 11:58:00 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 11:58:59 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    token.cpp
 * @brief   Implements Token helpers for the config lexer.
 *
 * @details Provides the Token constructor and lightweight debug helpers used to
 *          print token kinds and annotated token strings for diagnostics.
 *
 * @ingroup config_tokenizing
 */

#include "config/tokenizer/token.hpp"

#include <sstream>

//=== Construction ========================================================

/**
 * @brief Constructs a Token with type, value and source coordinates.
 *
 * @param t   Token type.
 * @param v   Lexeme string (unescaped for STRING).
 * @param l   1-based line number where the token begins.
 * @param c   1-based column number where the token begins.
 * @param off Byte offset into the original input.
 * @ingroup config_tokenizing
 */
Token::Token(TokenType t, const std::string& v, int l, int c, std::size_t off)
    : type(t), value(v), line(l), column(c), offset(off) {
}

//=== Debug Helpers =======================================================

/**
 * @brief Returns a compact string name for a TokenType.
 *
 * @param type Token category.
 * @return Short constant name (e.g., `"IDENTIFIER"`, `"LBRACE"`).
 * @ingroup config_tokenizing
 */
std::string debugTokenType(TokenType type) {
    switch (type) {
    case TokenType::IDENTIFIER:
        return "IDENTIFIER";
    case TokenType::NUMBER:
        return "NUMBER";
    case TokenType::STRING:
        return "STRING";
    case TokenType::LBRACE:
        return "LBRACE";
    case TokenType::RBRACE:
        return "RBRACE";
    case TokenType::SEMICOLON:
        return "SEMICOLON";
    case TokenType::COMMA:
        return "COMMA";
    case TokenType::END_OF_FILE:
        return "END_OF_FILE";

    case TokenType::KEYWORD_SERVER:
        return "KEYWORD_SERVER";
    case TokenType::KEYWORD_LOCATION:
        return "KEYWORD_LOCATION";
    case TokenType::KEYWORD_LISTEN:
        return "KEYWORD_LISTEN";
    case TokenType::KEYWORD_HOST:
        return "KEYWORD_HOST";
    case TokenType::KEYWORD_ROOT:
        return "KEYWORD_ROOT";
    case TokenType::KEYWORD_INDEX:
        return "KEYWORD_INDEX";
    case TokenType::KEYWORD_AUTOINDEX:
        return "KEYWORD_AUTOINDEX";
    case TokenType::KEYWORD_METHODS:
        return "KEYWORD_METHODS";
    case TokenType::KEYWORD_UPLOAD_STORE:
        return "KEYWORD_UPLOAD_STORE";
    case TokenType::KEYWORD_RETURN:
        return "KEYWORD_RETURN";
    case TokenType::KEYWORD_ERROR_PAGE:
        return "KEYWORD_ERROR_PAGE";
    case TokenType::KEYWORD_CLIENT_MAX_BODY_SIZE:
        return "KEYWORD_CLIENT_MAX_BODY_SIZE";
    case TokenType::KEYWORD_CGI_EXTENSION:
        return "KEYWORD_CGI_EXTENSION";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief Builds a human-readable token string (for logs and errors).
 *
 * @details Omits overly large payloads to avoid noisy logs.
 *
 * @param token Token to stringify.
 * @return A string like: `[Token type=IDENTIFIER value="root" line=3 column=5]`.
 * @ingroup config_tokenizing
 */
std::string debugToken(const Token& token) {
    // Avoid logging large token payloads to prevent excessive output
    if (token.value.size() > 1024 * 1024) {
        return "[Token <value too large to print>]";
    }

    std::ostringstream oss;
    oss << "[Token type=" << debugTokenType(token.type) << " value=\"" << token.value
        << "\" line=" << token.line << " column=" << token.column << "]";
    return oss.str();
}
