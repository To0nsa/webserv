/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Tokenizer.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/04 13:17:47 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/21 21:36:52 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "token.hpp"
#include <string>
#include <vector>

class Tokenizer {
  public:
    ////////////////////
    // --- Constructors
    explicit Tokenizer(std::string input);
    Tokenizer(const Tokenizer&)                = delete;
    Tokenizer& operator=(const Tokenizer&)     = delete;
    Tokenizer(Tokenizer&&) noexcept            = default;
    Tokenizer& operator=(Tokenizer&&) noexcept = default;

    ////////////////
    // --- Main API
    [[nodiscard]] std::vector<Token> tokenize();

    ////////////////////
    // --- Token Access
    std::string extractLine(std::size_t offset) const;

  private:
    /////////////////////////
    // --- Core Cursor Logic
    bool          isAtEnd() const noexcept;
    bool          match(char expected) noexcept;
    unsigned char peek() const noexcept;
    unsigned char peekNext() const noexcept;
    unsigned char advance() noexcept;

    ////////////////////////////
    // --- Classification Logic
    inline bool isIdentifierStart(unsigned char c) const;
    inline bool isIdentifierChar(unsigned char c) const;

    //////////////////////////
    // --- High-Level Parsers
    void      skipUtf8BOM();
    void      skipWhitespaceAndComments();
    TokenType resolveKeywordType(const std::string& word);
    void      scanIdentifier();
    void      validateIdentifier(std::size_t start);
    Token     parseIdentifierOrKeyword();

    /////////////////////////////
    // --- Number & Unit Parsing
    void  scanDigits();
    void  scanOptionalUnitSuffix();
    Token parseNumberOrUnit();

    //////////////////////
    // --- String Parsing
    void  throwUnterminatedString(const std::string& reason);
    Token parseStringLiteral();

    ////////////////////////////////////
    // --- Whitespace & Comment Helpers
    void skipCR();
    void skipNewline();
    void skipOtherWhitespace();
    void skipHashComment();

    //////////////////////
    // --- Token Dispatch
    bool looksLikeIpAddress() const;
    void dispatchToken();

    ////////////////////////////////////
    // --- Token Creation
    Token makeToken(TokenType type, const std::string& value) const;

    //////////////////////
    // --- Internal State
    std::string        _input;      ///< Raw input string to tokenize.
    std::vector<Token> _tokens;     ///< Accumulated list of emitted tokens.
    std::size_t        _pos    = 0; ///< Current byte offset in the input.
    int                _line   = 1; ///< Current line number (1-based).
    int                _column = 1; ///< Current column number (1-based).
};
