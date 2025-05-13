/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Tokenizer.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/04 13:17:47 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/08 17:09:43 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Tokenizer.hpp
 * @brief   Defines the Tokenizer class for lexical analysis of configuration files.
 *
 * @details
 * This file declares the `Tokenizer` class, which is responsible for reading and tokenizing
 * the configuration file input. It converts raw input into a sequence of `Token` objects based
 * on defined syntax, including identifiers, keywords, literals, and punctuation. The tokenizer
 * plays a key role in breaking down the configuration file into manageable pieces for further
 * parsing and validation.
 *
 * The `Tokenizer` supports handling different token types, whitespace, comments, and special
 * cases like UTF-8 BOM detection. It provides detailed error handling for invalid syntax or
 * unexpected input.
 *
 * @ingroup config
 */

#pragma once

#include "token.hpp"
#include <string>
#include <vector>

class Tokenizer {
  public:
    ////////////////////
    // --- Constructors
    /**
     * @brief Constructs a Tokenizer for the given input string.
     *
     * @details Initializes the tokenizer with the provided configuration file input.
     *          The input string will be tokenized into a sequence of `Token` objects,
     *          which are used for lexical analysis during configuration parsing.
     *          The constructor sets up internal state like the current position, line,
     *          and column number for efficient token tracking.
     *
     * @param input The raw configuration input to tokenize.
     * @ingroup config
     */
    explicit Tokenizer(std::string input);
    Tokenizer(const Tokenizer&)                = delete;
    Tokenizer& operator=(const Tokenizer&)     = delete;
    Tokenizer(Tokenizer&&) noexcept            = default;
    Tokenizer& operator=(Tokenizer&&) noexcept = default;

    ////////////////
    // --- Main API
    /**
     * @brief Tokenizes the input string into a sequence of tokens.
     *
     * @details Processes the input configuration string and breaks it into a series of
     *          `Token` objects, each representing a meaningful unit in the input (e.g.,
     *          identifiers, numbers, strings, keywords, and syntactic symbols). This method
     *          performs lexical analysis, skipping whitespace and comments, and returns a
     *          vector of tokens for further parsing.
     *
     * @return A vector of `Token` objects representing the tokenized input.
     * @note [[nodiscard]] This method returns a value that should not be ignored.
     * @ingroup config
     */
    [[nodiscard]] std::vector<Token> tokenize();

    ////////////////////
    // --- Token Access
    /**
     * @brief Extracts a specific line from the input based on the given offset.
     *
     * @details This method retrieves the line of text from the input configuration
     *          starting from a given offset. It handles edge cases such as lines
     *          starting before the offset or ending after it, ensuring that the entire
     *          line of text is returned for diagnostic purposes.
     *
     * @param offset The byte offset in the input where the line starts.
     * @return The string representing the line containing the given offset.
     * @ingroup config
     */
    std::string extractLine(std::size_t offset) const;

  private:
    /////////////////////////
    // --- Core Cursor Logic
    /**
     * @brief Checks if the tokenizer has reached the end of the input.
     *
     * @details This method returns `true` if the tokenizer has processed all the
     *          characters in the input string. It is useful for determining when
     *          the tokenization process has completed and no further tokens are available.
     *
     * @return `true` if the tokenizer has reached the end of the input, `false` otherwise.
     * @ingroup config
     */
    bool isAtEnd() const noexcept;
    /**
     * @brief Checks if the current character matches the expected character.
     *
     * @details This method compares the current character in the input with the
     *          provided `expected` character. If they match, the tokenizer's
     *          position is advanced by one character, and the method returns `true`.
     *          If they do not match, the method returns `false` without advancing
     *          the position.
     *
     * @param expected The character to compare the current input character with.
     * @return `true` if the current character matches `expected`, `false` otherwise.
     * @ingroup config
     */
    bool match(char expected) noexcept;
    /**
     * @brief Returns the current character without advancing the position.
     *
     * @details This method provides a look-ahead feature, allowing you to view
     *          the next character in the input string without moving the tokenizer's
     *          position. This is useful for inspection and comparison before deciding
     *          how to process the character.
     *
     * @return The current character in the input.
     * @ingroup config
     */
    unsigned char peek() const noexcept;
    /**
     * @brief Returns the next character in the input without advancing the position.
     *
     * @details This method allows looking ahead by one character in the input string,
     *          without consuming it. If the tokenizer is at the end of the input,
     *          it will return a null character (`'\0'`).
     *
     * @return The next character in the input, or `'\0'` if at the end of the input.
     * @ingroup config
     */
    unsigned char peekNext() const noexcept;
    /**
     * @brief Advances the tokenizer's position by one character.
     *
     * @details This method consumes the current character and moves the tokenizer's
     *          position forward. It updates the line and column numbers accordingly.
     *          If a newline character is encountered, the line number is incremented,
     *          and the column number is reset to 1.
     *
     * @return The character that was consumed.
     * @ingroup config
     */
    char advance() noexcept;

    ////////////////////////////
    // --- Classification Logic
    /**
     * @brief Checks if a character is a valid start for an identifier.
     *
     * @details
     * Accepts alphabetic characters, underscore (`_`), slash (`/`), dot (`.`),
     * hyphen (`-`), and colon (`:`) as valid starting characters.
     * Used when parsing identifiers or paths.
     *
     * @param c The character to check.
     * @return True if the character is a valid identifier start.
     * @ingroup config
     */
    inline bool isIdentifierStart(unsigned char c) const;
    /**
     * @brief Checks if a character is valid within an identifier.
     *
     * @details
     * Accepts alphanumeric characters, underscore (`_`), slash (`/`), dot (`.`),
     * hyphen (`-`), and colon (`:`). This is a superset of valid starting characters.
     *
     * @param c The character to check.
     * @return True if the character is allowed inside an identifier.
     * @ingroup config
     */
    inline bool isIdentifierChar(unsigned char c) const;

    //////////////////////////
    // --- High-Level Parsers
    /**
     * @brief Skips a UTF-8 Byte Order Mark (BOM) if present at the start of input.
     *
     * @details
     * Checks for the BOM sequence (`0xEF 0xBB 0xBF`) at the beginning of the input
     * and advances the cursor past it. This ensures compatibility with editors
     * that prepend BOMs to UTF-8 files.
     * @ingroup config
     */
    void skipUtf8BOM();

    //////////////////////////////////////
    // --- Skipping Whitespace & Comments
    /**
     * @brief Skips over all whitespace and comments until meaningful input is found.
     *
     * @details
     * Advances the cursor past spaces, tabs, newlines, and all supported comment styles:
     * - `#` comments (shell-style)
     * - `//` comments (C++-style)
     * - `/ * * /` block comments (C-style, supports multiline)
     *
     * Ensures that tokenization begins at the next significant character.
     * @ingroup config
     */
    void skipWhitespaceAndComments();

    ////////////////////////////////////
    // --- Keyword & Identifier Parsing
    /**
     * @brief Determines if a word is a recognized keyword or a generic identifier.
     *
     * @details
     * Checks if the given word matches any known configuration keywords.
     * Returns the appropriate `TokenType`. If the word is not a keyword but
     * contains uppercase letters, throws a `TokenizerError` to enforce lowercase-only rules.
     *
     * @param word The candidate word to classify.
     * @return A `TokenType` corresponding to a keyword or `IDENTIFIER`.
     *
     * @throws TokenizerError If the word is a keyword written with uppercase letters.
     * @ingroup config
     */
    TokenType resolveKeywordType(const std::string& word);
    /**
     * @brief Advances the cursor over a complete identifier.
     *
     * @details
     * Assumes the current character is a valid identifier start.
     * Continues advancing while characters are valid identifier characters.
     * Does not return a value — modifies the internal cursor state.
     * @ingroup config
     */
    void scanIdentifier();
    /**
     * @brief Validates the identifier scanned from a given start position.
     *
     * @details
     * Ensures the scanned range is non-empty, well-formed, and does not contain
     * control or non-printable characters. Throws a `TokenizerError` if the
     * identifier is malformed or empty.
     *
     * @param start The starting offset of the identifier in the input buffer.
     *
     * @throws TokenizerError If the identifier is invalid or contains illegal characters.
     * @ingroup config
     */
    void validateIdentifier(std::size_t start);
    /**
     * @brief Parses an identifier or keyword token from the current position.
     *
     * @details
     * Scans a sequence of valid identifier characters starting from the current
     * position, then determines whether the resulting string matches a known
     * configuration keyword (case-insensitively). If so, returns a `Token`
     * with the corresponding `TokenType`; otherwise, returns it as a generic
     * `TokenType::IDENTIFIER`.
     *
     * This function also performs identifier validation to ensure the token
     * contains only printable, non-control characters.
     *
     * @return A `Token` of type `KEYWORD_*` or `IDENTIFIER`.
     *
     * @throws TokenizerError If the identifier is malformed or invalid.
     * @ingroup config
     */
    Token parseIdentifierOrKeyword();

    /////////////////////////////
    // --- Number & Unit Parsing
    /**
     * @brief Advances the cursor over a sequence of digit characters.
     *
     * @details
     * Assumes the current character is a digit.
     * Continues advancing while characters are in the range `'0'` to `'9'`.
     * Does not return a value — updates the cursor position internally.
     * @ingroup config
     */
    void scanDigits();
    /**
     * @brief Scans and consumes an optional unit suffix (e.g., 'k', 'm', 'g').
     *
     * @details
     * If the current character is an alphabetic letter, consumes it as a potential
     * unit suffix (e.g., `10k`, `5M`, `1g`). If a second letter follows immediately,
     * throws a `TokenizerError` to reject invalid multi-letter suffixes like `10mb`.
     *
     * @throws TokenizerError If the suffix contains more than one alphabetic character.
     * @ingroup config
     */
    void scanOptionalUnitSuffix();
    /**
     * @brief Parses a numeric token, optionally followed by a size suffix.
     *
     * @details
     * Scans a contiguous sequence of digits and an optional single-letter
     * unit suffix (`k`, `m`, `g`). Validates that no invalid characters follow.
     * Returns a `Token` of type `NUMBER` with the raw string (e.g., `"64k"`).
     *
     * @return A `Token` representing the numeric or byte-size literal.
     *
     * @throws TokenizerError If the input contains a malformed number or suffix.
     * @ingroup config
     */
    Token parseNumberOrUnit();
    /**
     * @brief Parses and returns a single escape sequence inside a string literal.
     *
     * @details
     * Handles common C-style escapes (`\\`, `\n`, `\t`, `\"`, etc.).
     * Assumes the leading backslash has already been consumed.
     * Throws if the escape is invalid or unsupported in the context of the given quote type.
     *
     * @param quote The type of surrounding quote (`'` or `"`), used to restrict escape usage.
     * @return The resolved escape sequence as a string (usually 1 character).
     *
     * @throws TokenizerError If the escape sequence is invalid or unsupported.
     * @ingroup config
     */

    //////////////////////
    // --- String Parsing
    std::string parseEscapeSequence(unsigned char quote);
    /**
     * @brief Throws a TokenizerError for an unterminated string literal.
     *
     * @details
     * Used when a string literal reaches an unexpected end (e.g., newline or EOF)
     * before the closing quote. Appends the provided reason to clarify the context.
     *
     * @param reason A short explanation for why the string was considered unterminated.
     *
     * @throws TokenizerError Always throws with a detailed message and source snippet.
     * @ingroup config
     */
    void throwUnterminatedString(const std::string& reason);
    /**
     * @brief Parses a double-quoted or single-quoted string literal.
     *
     * @details
     * Supports escape sequences (e.g., `\n`, `\\`, `\"`) only in double-quoted strings.
     * Single-quoted strings must contain raw characters without escapes.
     * Enforces a maximum string length of 64 KiB and rejects unterminated or multiline strings.
     *
     * @return A `Token` of type `STRING` with the parsed content.
     *
     * @throws TokenizerError If the string is invalid, unterminated, or exceeds size limits.
     * @ingroup config
     */
    Token parseStringLiteral();

    ////////////////////////////////////
    // --- Whitespace & Comment Helpers
    /**
     * @brief Skips a carriage return character (`'\r'`) in the input.
     *
     * @details
     * Used to handle Windows-style line endings (`\r\n`). Does not advance line count,
     * only moves the cursor forward by one position.
     * @ingroup config
     */
    void skipCR();
    /**
     * @brief Skips a newline character (`'\n'`) and updates line tracking.
     *
     * @details
     * Advances the input cursor past a newline and increments the internal line counter.
     * Resets the column counter to 1. Used for Unix-style line endings and after `\r\n`.
     * @ingroup config
     */
    void skipNewline();
    /**
     * @brief Skips a single whitespace character that is not a newline or carriage return.
     *
     * @details
     * Advances the cursor and column counter by one. Intended for characters like space (`' '`)
     * or tab (`'\t'`) during general whitespace skipping.
     * @ingroup config
     */
    void skipOtherWhitespace();
    /**
     * @brief Skips a C++-style comment starting with `//`.
     *
     * @details
     * Advances the cursor past the `//` and continues until a newline or end of input.
     * Used to ignore single-line comments during tokenization.
     * @ingroup config
     */
    void skipDoubleSlashComment();
    /**
     * @brief Skips a shell-style comment starting with `#`.
     *
     * @details
     * Advances the cursor past the `#` and continues until a newline or end of input.
     * Commonly used in configuration files as an alternative to `//`.
     * @ingroup config
     */
    void skipHashComment();
    /**
     * @brief Skips a C-style block comment (`/ * ... * /`).
     *
     * @details
     * Advances the cursor past the opening `/ *` and continues until the closing `* /`.
     * Tracks newlines to maintain accurate line and column info.
     * If the comment is unterminated (i.e., reaches EOF first), throws a `TokenizerError`.
     *
     * @throws TokenizerError If the block comment is not properly closed.
     * @ingroup config
     */
    void skipMultiLineComment();

    //////////////////////
    // --- Token Dispatch
    /**
     * @brief Checks if the current token resembles an IP address.
     *
     * @details
     * Scans forward from the current position and returns `true` if the upcoming
     * characters match the pattern of an IPv4 address — i.e., digits separated
     * by at least two dots (e.g., `127.0.0.1`). This is used to avoid
     * misclassifying such sequences as numeric literals during token dispatch.
     *
     * This function is a heuristic and does not perform full IP validation.
     *
     * @return `true` if the upcoming characters look like a dotted IP address.
     * @ingroup config
     */
    bool looksLikeIpAddress() const;
    /**
     * @brief Dispatches and parses the next token from the input stream.
     *
     * @details
     * Inspects the current character and delegates to the appropriate parsing routine:
     * - Digits:
     *     - If it resembles an IP address (e.g., 127.0.0.1), dispatch as `IDENTIFIER`
     *     - Otherwise, parse as numeric literal via `parseNumberOrUnit()`
     * - Identifier start characters (`a`-`z`, `_`, `/`, `.`, `-`, `:`) →
     * `parseIdentifierOrKeyword()`
     * - Quotes (`'`, `"`) → `parseStringLiteral()`
     * - Symbols (`{`, `}`, `;`) → handled inline
     *
     * Pushes the resulting token into the internal `_tokens` vector.
     *
     * @throws TokenizerError If the character is unexpected or malformed.
     * @ingroup config
     */
    void dispatchToken();

    ////////////////////////////////////
    // --- Token Creation
    /**
     * @brief Creates a `Token` object with the given type and value.
     *
     * @details
     * Attaches current line and column information to the token.
     * Adjusts the column to point to the start of the token based on its length.
     *
     * @param type  The type of the token (e.g., IDENTIFIER, STRING).
     * @param value The raw string value of the token.
     * @return A fully constructed `Token` with type, value, line, and column.
     * @ingroup config
     */
    Token makeToken(TokenType type, const std::string& value) const;

    //////////////////////
    // --- Internal State
    std::string        _input;      ///< Raw input string to tokenize.
    std::vector<Token> _tokens;     ///< Accumulated list of emitted tokens.
    std::size_t        _pos    = 0; ///< Current byte offset in the input.
    int                _line   = 1; ///< Current line number (1-based).
    int                _column = 1; ///< Current column number (1-based).
};
