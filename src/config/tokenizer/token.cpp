/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   token.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/04 00:19:57 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/08 16:55:53 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    token.cpp
 * @brief   Implements the Token class and debug utilities for token inspection.
 *
 * @details This file defines the `Token` constructor and helper functions
 * such as `debugTokenType` and `debugToken` used for logging and diagnostics
 * during the tokenization and parsing of the configuration language.
 * @ingroup config
 */

#include "config/tokenizer/token.hpp"
#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

Token::Token(TokenType t, const std::string& v, int l, int c, std::size_t off)
    : type(t), value(v), line(l), column(c), offset(off) {
}

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
    case TokenType::END_OF_FILE:
        return "END_OF_FILE";

    // Keywords
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

std::string debugToken(const Token& token) {
    // Avoid logging large token payloads to prevent excessive output
    if (token.value.size() > 1024 * 1024) {
        return "[Token <value too large to print>]"; // Return a placeholder for large tokens
    }

    std::ostringstream oss;
    // Build the string representation of the token with its type, value, line, and column
    oss << "[Token type=" << debugTokenType(token.type) << " value=\"" << token.value
        << "\" line=" << token.line << " column=" << token.column << "]";
    return oss.str(); // Return the formatted token string
}
