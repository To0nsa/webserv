/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Tokenizer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/03 01:06:09 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/21 21:09:15 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/tokenizer/Tokenizer.hpp"
#include "config/parser/ConfigParseError.hpp"
#include "utils/errorUtils.hpp"
#include "utils/stringUtils.hpp"

#include <cctype>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

////////////////////////////////
// --- Constructors

Tokenizer::Tokenizer(std::string input) : _input(std::move(input)), _pos(0), _line(1), _column(1) {
}

////////////////
// --- Main API

std::vector<Token> Tokenizer::tokenize() {
    skipUtf8BOM(); // Skip UTF-8 BOM if present at the beginning

    _tokens.clear();                    // Ensure token list is empty before starting
    _tokens.reserve(_input.size() / 4); // Preallocate space to reduce reallocations

    while (!isAtEnd()) {
        skipWhitespaceAndComments(); // Skip spaces, comments, newlines
        if (isAtEnd())
            break;       // Avoid processing after end of input
        dispatchToken(); // Parse and append the next token
    }

    _tokens.push_back(makeToken(TokenType::END_OF_FILE, "")); // Final EOF token
    return _tokens;
}

/////////////////////////
// --- Core Cursor Logic

bool Tokenizer::isAtEnd() const noexcept {
    // True if the cursor has reached or passed end of input
    return _pos >= _input.size();
}

bool Tokenizer::match(char expected) noexcept {
    // Return false if at end or current char does not match
    if (isAtEnd() || _input[_pos] != expected) {
        return false;
    }
    // Advance position and column if matched
    ++_pos;
    ++_column;
    return true;
}

unsigned char Tokenizer::peek() const noexcept {
    // Return current character without consuming it
    return static_cast<unsigned char>(_input[_pos]);
}

unsigned char Tokenizer::peekNext() const noexcept {
    if (isAtEnd()) {
        return '\0';
    }
    return static_cast<unsigned char>(_input[_pos + 1]);
}

char Tokenizer::advance() noexcept {
    char c = _input[_pos++]; // Consume current character and move cursor forward
    if (c == '\n') {
        _line++;     // Increment line number on newline
        _column = 1; // Reset column to start of line
    } else {
        _column++; // Otherwise, move to next column
    }
    return c; // Return the consumed character
}

////////////////////////////
// --- Classification Logic

inline bool Tokenizer::isIdentifierStart(unsigned char c) const {
    // Valid first char for identifiers
    return std::isalpha(c) || c == '_' || c == '/' || c == '.' || c == '-' || c == ':';
}

inline bool Tokenizer::isIdentifierChar(unsigned char c) const {
    // Valid body char for identifiers
    return std::isalnum(c) || c == '_' || c == '/' || c == '.' || c == '-' || c == ':';
}

//////////////////////////
// --- High-Level Parsers

////////////////////////////////
// --- Skip BOM
void Tokenizer::skipUtf8BOM() {
    static const std::string BOM = "\xEF\xBB\xBF"; // UTF-8 Byte Order Mark
    if (_input.compare(0, BOM.size(), BOM) == 0) {
        _pos += BOM.size(); // Skip BOM if present at the beginning of input
    }
}

//////////////////////////////////
// --- Skip Whitespace & Comments
void Tokenizer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        unsigned char c = peek();

        if (c == '\r') {
            skipCR();
        } else if (c == '\n') {
            skipNewline();
        } else if (std::isspace(c)) {
            skipOtherWhitespace();
        } else if (c == '#') {
            skipHashComment();
        } else {
            return; // Stop when encountering non-whitespace, non-comment character
        }
    }
}
///////////////////////////////////
// ---Identifier & Keyword Parsing

TokenType Tokenizer::resolveKeywordType(const std::string& word) {
    // Static map of all recognized configuration keywords (lowercase only)
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"server", TokenType::KEYWORD_SERVER},
        {"location", TokenType::KEYWORD_LOCATION},
        {"listen", TokenType::KEYWORD_LISTEN},
        {"host", TokenType::KEYWORD_HOST},
        {"root", TokenType::KEYWORD_ROOT},
        {"index", TokenType::KEYWORD_INDEX},
        {"autoindex", TokenType::KEYWORD_AUTOINDEX},
        {"methods", TokenType::KEYWORD_METHODS},
        {"upload_store", TokenType::KEYWORD_UPLOAD_STORE},
        {"return", TokenType::KEYWORD_RETURN},
        {"error_page", TokenType::KEYWORD_ERROR_PAGE},
        {"client_max_body_size", TokenType::KEYWORD_CLIENT_MAX_BODY_SIZE},
        {"cgi_extension", TokenType::KEYWORD_CGI_EXTENSION},
    };

    // Convert the input word to lowercase to support case-insensitive keyword matching
    std::string lower;
    lower.reserve(word.size());
    for (char c : word)
        lower += std::tolower(static_cast<unsigned char>(c));

    // Look up the lowercase version in the keyword map
    std::unordered_map<std::string, TokenType>::const_iterator it = keywords.find(lower);
    if (it != keywords.end()) {
        return it->second; // Found a keyword — return its associated TokenType
    }

    // Not a known keyword — treat as a generic IDENTIFIER
    return TokenType::IDENTIFIER;
}

void Tokenizer::scanIdentifier() {
    // Assumes the current character is a valid identifier start (checked beforehand)
    while (!isAtEnd() && isIdentifierChar(peek())) {
        advance(); // Consume characters as long as they're valid in an identifier
    }
}

void Tokenizer::validateIdentifier(std::size_t start) {
    // Reject empty identifiers
    if (_pos == start) {
        throw TokenizerError(formatError("Zero-length identifier", _line, _column),
                             extractLine(_pos));
    }

    // Sanity check: start must be before _pos
    if (_pos < start) {
        throw TokenizerError(formatError("Internal error: invalid token range", _line, _column),
                             extractLine(_pos));
    }

    // Create a view of the scanned identifier
    const std::string_view word(_input.data() + start, _pos - start);

    // Reject unquoted '$'
    if (word.find('$') != std::string_view::npos) {
        throw TokenizerError(
            formatError(
                "Found '$' in unquoted token; please wrap any text containing '$' in quotes", _line,
                _column),
            extractLine(_pos));
    }

    // Reject non-printable/control characters (ASCII < 0x20 or DEL = 0x7F)
    for (char c : word) {
        if (!std::isprint(static_cast<unsigned char>(c))) {
            throw TokenizerError(
                formatError("Identifier contains non-printable/control character", _line, _column),
                extractLine(_pos));
        }
    }
}

Token Tokenizer::parseIdentifierOrKeyword() {
    std::size_t start = _pos;  // Record start position of the identifier
    scanIdentifier();          // Consume all valid identifier characters
    validateIdentifier(start); // Ensure it's non-empty and well-formed
    std::string word = _input.substr(start, _pos - start); // Extract the identifier text

    // Determine if it's a keyword or generic identifier and return the token
    return makeToken(resolveKeywordType(word), word);
}

/////////////////////////////
// --- Number & Unit Parsing
void Tokenizer::scanDigits() {
    // Consume characters as long as they are digits (0–9)
    while (!isAtEnd() && std::isdigit(peek())) {
        advance(); // Advance cursor for each digit
    }
}

void Tokenizer::scanOptionalUnitSuffix() {
    if (isAtEnd())
        return; // Nothing to scan

    unsigned char c = peek();
    if (std::isalpha(c)) {
        advance(); // Accept single-letter suffix (e.g., 'k', 'm', 'g')

        // Reject multi-letter suffixes like "mb", "MiB", etc.
        if (!isAtEnd() && std::isalpha(peek())) {
            throw TokenizerError(
                formatError("Invalid number suffix: expected single letter like 'k', 'm', or 'g'",
                            _line, _column),
                extractLine(_pos));
        }
    }
}

Token Tokenizer::parseNumberOrUnit() {
    std::size_t start = _pos; // Record start of numeric token

    scanDigits();             // Consume all digit characters
    scanOptionalUnitSuffix(); // Optionally consume a unit suffix like 'k', 'm', or 'g'

    // Create a NUMBER token from the scanned substring
    return makeToken(TokenType::NUMBER, _input.substr(start, _pos - start));
}

//////////////////////
// --- String Parsing
void Tokenizer::throwUnterminatedString(const std::string& reason) {
    // Throw a TokenizerError with a contextual reason and source line
    throw TokenizerError(formatError("Invalid string literal (" + reason + ")", _line, _column),
                         extractLine(_pos));
}

Token Tokenizer::parseStringLiteral() {
    static const std::size_t MAX_STRING_LITERAL_LENGTH = 64 * 1024; // 64 KiB

    unsigned char quote = advance(); // Consume opening quote (' or ")
    std::string   content;

    while (!isAtEnd()) {
        unsigned char c = peek();

        if (c == '\n') {
            throwUnterminatedString("unexpected newline");
        }

        c = advance(); // Consume character

        if (c == quote) {
            return makeToken(TokenType::STRING, content); // Closing quote — done
        }

        content += c; // Append literally (includes backslash)

        if (content.size() > MAX_STRING_LITERAL_LENGTH) {
            throw TokenizerError(formatError("String literal exceeds 64 KiB limit", _line, _column),
                                 extractLine(_pos));
        }
    }

    throwUnterminatedString("end of input");                // Reached EOF before closing quote
    return Token(TokenType::STRING, "", _line, _column, 0); // Unreachable
}

////////////////////////////////////
// --- Whitespace & Comment Helpers

void Tokenizer::skipCR() {
    ++_pos;
}

void Tokenizer::skipNewline() {
    ++_pos;
    ++_line;
    _column = 1;
}

void Tokenizer::skipOtherWhitespace() {
    ++_pos;
    ++_column;
}

void Tokenizer::skipHashComment() {
    ++_pos;
    ++_column;
    while (!isAtEnd() && peek() != '\n') {
        ++_pos;
        ++_column;
    }
}

//////////////////////
// --- Token Dispatch
bool Tokenizer::looksLikeIpAddress() const {
    std::size_t len = _input.size();
    std::size_t i = _pos, dots = 0;

    while (i < len) {
        char c = _input[i];

        if (std::isdigit(c)) {
            ++i; // Continue if character is a digit
        } else if (c == '.') {
            ++dots; // Count a dot separator
            ++i;
        } else {
            break; // Stop on any other character (e.g. space, semicolon, alpha)
        }
    }

    // A valid IPv4 address has at least two dots (e.g., 127.0.0.1)
    return dots == 3;
    // A valid IPv4 address has at least three dots (e.g., 127.0.0.1)
    return dots >= 3;
}

void Tokenizer::dispatchToken() {
    unsigned char c = peek();

    // Handle tokens that start with a digit
    if (std::isdigit(c)) {
        unsigned char next = peekNext();

        // If it looks like an IPv4 address (e.g. 127.0.0.1), or
        // if the next character is *not* a digit but *is* a valid identifier char
        // ("1index.html" or "/api/v2a3"), then parse as an identifier/keyword
        if (looksLikeIpAddress() || (!std::isdigit(next) && isIdentifierChar(next))) {
            _tokens.push_back(parseIdentifierOrKeyword());
        } else {
            // Otherwise treat it as a numeric literal (possibly with 'k','m','g' suffix)
            _tokens.push_back(parseNumberOrUnit());
        }

        // Handle identifiers or keywords (must start with letter, '_', '-', '.', or '/')
    } else if (isIdentifierStart(c)) {
        _tokens.push_back(parseIdentifierOrKeyword());

        // Handle quoted strings
    } else if (c == '"' || c == '\'') {
        _tokens.push_back(parseStringLiteral());

        // Handle single-character tokens
    } else {
        switch (c) {
        case '{':
            advance();
            _tokens.push_back(makeToken(TokenType::LBRACE, "{"));
            break;
        case '}':
            advance();
            _tokens.push_back(makeToken(TokenType::RBRACE, "}"));
            break;
        case ';':
            advance();
            _tokens.push_back(makeToken(TokenType::SEMICOLON, ";"));
            break;
        case ',':
            advance();
            _tokens.push_back(makeToken(TokenType::COMMA, ","));
            break;
        default:
            // Any other character is invalid here
            throw TokenizerError(
                formatError("Unexpected character '" + std::string(1, static_cast<char>(c)) + "'",
                            _line, _column),
                extractLine(_pos));
        }
    }
}

////////////////////////////////////
// --- Token Creation & Source Info
Token Tokenizer::makeToken(TokenType type, const std::string& value) const {
    // Estimate starting column based on current column and token length
    int col = _column - static_cast<int>(value.length());
    if (col < 1) {
        col = 1; // Clamp to column 1 to avoid negative/zero values
    }
    std::size_t off = _pos - value.size(); // Calculate byte offset in the original input string
    // Construct and return a token with current line and computed column
    return Token(type, value, _line, col, off);
}

std::string Tokenizer::extractLine(std::size_t offset) const {
    // Find the last newline before (or at) the offset to locate the start of the line
    std::size_t start = _input.rfind('\n', offset);
    if (start == std::string::npos)
        start = 0; // If no newline was found, the line starts at the beginning
    else
        start += 1; // Move past the newline character to the actual start of the line

    // Find the next newline after the start to locate the end of the current line
    std::size_t end = _input.find('\n', start);
    if (end == std::string::npos)
        end = _input.size(); // If no newline found, the line ends at the end of the input

    // Extract and return the substring representing the line containing the offset
    return _input.substr(start, end - start);
}
