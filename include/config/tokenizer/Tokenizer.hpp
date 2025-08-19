/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Tokenizer.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/04 13:17:47 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 19:46:40 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Tokenizer.hpp
 * @brief   Declares the Tokenizer for config lexical analysis.
 *
 * @details Converts raw configuration text into a typed token stream with
 *          accurate line/column offsets for diagnostics. Recognizes keywords,
 *          identifiers, numbers with single-letter size suffixes, strings,
 *          punctuation, and skips comments/whitespace/BOM.
 *
 * @ingroup config_tokenizing
 */

#pragma once

#include "token.hpp" // for Token, TokenType
#include <cstddef>   // for size_t
#include <string>    // for string
#include <vector>    // for vector

/**
 * @brief Stateful lexer for the configuration language.
 *
 * @details Produces a vector of @ref Token from an input string. The lexer is
 *          single-pass and records source positions for error reporting. It is
 *          intentionally strict (e.g., rejects multi-letter numeric suffixes).
 *
 * @ingroup config_tokenizing
 */
class Tokenizer {
  public:
    //=== Construction & Special Members =====================================

    /** @name Construction & special members */
    ///@{
    explicit Tokenizer(std::string input);
    Tokenizer(const Tokenizer&)                = delete;
    Tokenizer& operator=(const Tokenizer&)     = delete;
    Tokenizer(Tokenizer&&) noexcept            = default;
    Tokenizer& operator=(Tokenizer&&) noexcept = default;
    ///@}

    //=== Main API ============================================================

    /** @name Main API */
    ///@{
    [[nodiscard]] std::vector<Token> tokenize();
    ///@}

    //=== Token Access Helpers ===============================================

    /** @name Token access helpers */
    ///@{
    std::string extractLine(std::size_t offset) const;
    ///@}

  private:
    //=== Core Cursor Logic ===================================================

    /** @name Core cursor logic */
    ///@{
    bool          isAtEnd() const noexcept;
    bool          match(char expected) noexcept;
    unsigned char peek() const noexcept;
    unsigned char peekNext() const noexcept;
    unsigned char advance() noexcept;
    ///@}

    //=== Classification Logic ===============================================

    /** @name Classification logic */
    ///@{
    inline bool isIdentifierStart(unsigned char c) const;
    inline bool isIdentifierChar(unsigned char c) const;
    ///@}

    //=== High-Level Parsers ==================================================

    /** @name High-level parsers */
    ///@{
    void      skipUtf8BOM();
    void      skipWhitespaceAndComments();
    TokenType resolveKeywordType(const std::string& word);
    void      scanIdentifier();
    void      validateIdentifier(std::size_t start);
    Token     parseIdentifierOrKeyword();
    ///@}

    //=== Number & Unit Parsing ==============================================

    /** @name Number & unit parsing */
    ///@{
    void  scanDigits();
    void  scanOptionalUnitSuffix();
    Token parseNumberOrUnit();
    ///@}

    //=== String Parsing ======================================================

    /** @name String parsing */
    ///@{
    void  throwUnterminatedString(const std::string& reason);
    Token parseStringLiteral();
    ///@}

    //=== Whitespace & Comment Helpers =======================================

    /** @name Whitespace & comment helpers */
    ///@{
    void skipCR();
    void skipNewline();
    void skipOtherWhitespace();
    void skipHashComment();
    ///@}

    //=== Token Dispatch ======================================================

    /** @name Token dispatch */
    ///@{
    bool looksLikeIpAddress() const;
    void dispatchToken();
    ///@}

    //=== Token Creation ======================================================

    /** @name Token creation */
    ///@{
    Token makeToken(TokenType type, const std::string& value) const;
    ///@}

    //=== Internal State ======================================================

    /** @name Internal state */
    ///@{
    std::string        _input;      ///< Raw input string to tokenize.
    std::vector<Token> _tokens;     ///< Accumulated list of emitted tokens.
    std::size_t        _pos    = 0; ///< Current byte offset in the input.
    int                _line   = 1; ///< Current line number (1-based).
    int                _column = 1; ///< Current column number (1-based).
    ///@}
};
