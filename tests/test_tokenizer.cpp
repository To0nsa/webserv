/**
 * @file    test_tokenizer.cpp
 * @brief   Unit tests for Token and Tokenizer classes.
 *
 * @details Covers valid token flows and critical failure modes:
 * malformed identifiers, unterminated strings, and bad comments.
 * @ingroup config
 */

#include "config/parser/ConfigParseError.hpp"
#include "config/tokenizer/Tokenizer.hpp"
#include "config/tokenizer/token.hpp"
#include <cassert>
#include <iostream>
#include <vector>

void testDebugTokenType() {
    assert(debugTokenType(TokenType::IDENTIFIER) == "IDENTIFIER");
    assert(debugTokenType(TokenType::KEYWORD_SERVER) == "KEYWORD_SERVER");
    assert(debugTokenType(static_cast<TokenType>(9999)) == "UNKNOWN");
}

void testDebugToken() {
    Token       t(TokenType::STRING, "hello", 3, 12, 42);
    std::string repr = debugToken(t);
    assert(repr.find("type=STRING") != std::string::npos);
    assert(repr.find("value=\"hello\"") != std::string::npos);
    assert(repr.find("line=3") != std::string::npos);
    assert(repr.find("column=12") != std::string::npos);
}

void testSimpleTokenization() {
    const std::string  input = R"(
		server {
			listen 8080;
			root "/var/www";
			location / {
				index "index.html";
				autoindex on;
			}
		}
	)";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();

    assert(!tokens.empty());
    assert(tokens.back().type == TokenType::END_OF_FILE);
}

void testUnterminatedStringThrows() {
    const std::string input = R"(root "unterminated)";
    try {
        Tokenizer tokenizer(input);
        (void) tokenizer.tokenize();
        assert(false && "Expected TokenizerError for unterminated string");
    } catch (const TokenizerError& e) {
        std::cerr << "✅ Caught expected unterminated string: " << e.what() << '\n';
    }
}

void testInvalidIdentifierThrows() {
    const std::string input = "listen \x01;";
    try {
        Tokenizer tokenizer(input);
        (void) tokenizer.tokenize();
        assert(false && "Expected TokenizerError for control char in identifier");
    } catch (const TokenizerError& e) {
        std::cerr << "✅ Caught expected invalid identifier error: " << e.what() << '\n';
    }
}

void testUnterminatedCommentThrows() {
    const std::string input = "server { /* unterminated comment ";
    try {
        Tokenizer tokenizer(input);
        (void) tokenizer.tokenize();
        assert(false && "Expected TokenizerError for unterminated comment");
    } catch (const TokenizerError& e) {
        std::cerr << "✅ Caught expected unterminated comment: " << e.what() << '\n';
    }
}

void testMultiLineCommentSkips() {
    const std::string  input = R"(
		server {
			/* this is a
			multi-line comment */
			listen 8080;
		}
	)";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();
    assert(tokens[0].type == TokenType::KEYWORD_SERVER);
    assert(tokens[1].type == TokenType::LBRACE);
    assert(tokens[2].type == TokenType::KEYWORD_LISTEN);
    assert(tokens[3].type == TokenType::NUMBER);
    assert(tokens[4].type == TokenType::SEMICOLON);
    assert(tokens[5].type == TokenType::RBRACE);
    assert(tokens[6].type == TokenType::END_OF_FILE);
}

void testValidEscapedString() {
    const std::string  input = R"(root "line\nbreak\tand\\slash";)";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();

    assert(tokens.size() >= 3);
    assert(tokens[0].type == TokenType::KEYWORD_ROOT);
    assert(tokens[1].type == TokenType::STRING);
    assert(tokens[1].value == "line\nbreak\tand\\slash");
    assert(tokens[2].type == TokenType::SEMICOLON);
}

void testInvalidEscapeInSingleQuotedString() {
    const std::string input = "root 'invalid\\escape';";
    try {
        Tokenizer tokenizer(input);
        (void) tokenizer.tokenize();
        assert(false && "Expected TokenizerError for backslash in single-quoted string");
    } catch (const TokenizerError& e) {
        std::cerr << "✅ Caught single-quoted string escape error: " << e.what() << '\n';
    }
}

void testInvalidDoubleEscapeSequence() {
    const std::string input = R"(root "invalid\qescape";)";
    try {
        Tokenizer tokenizer(input);
        (void) tokenizer.tokenize();
        assert(false && "Expected TokenizerError for unknown escape");
    } catch (const TokenizerError& e) {
        std::cerr << "✅ Caught bad escape sequence: " << e.what() << '\n';
    }
}

void testMaxStringLengthExceeded() {
    std::string long_str = "\"" + std::string(64 * 1024 + 1, 'A') + "\";";
    std::string input    = "root " + long_str;
    try {
        Tokenizer tokenizer(input);
        (void) tokenizer.tokenize();
        assert(false && "Expected TokenizerError for string > 64 KiB");
    } catch (const TokenizerError& e) {
        std::cerr << "✅ Caught oversized string literal: " << e.what() << '\n';
    }
}

void testInvalidNumberSuffix() {
    const std::string input = "client_max_body_size 10mb;";
    try {
        Tokenizer tokenizer(input);
        (void) tokenizer.tokenize();
        assert(false && "Expected TokenizerError for multi-letter suffix");
    } catch (const TokenizerError& e) {
        std::cerr << "✅ Caught invalid unit suffix: " << e.what() << '\n';
    }
}

void testHashCommentSkips() {
    const std::string  input = R"(
		server {
			# comment with text
			listen 8080;
		}
	)";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();
    assert(tokens[0].type == TokenType::KEYWORD_SERVER);
    assert(tokens[2].type == TokenType::KEYWORD_LISTEN);
    assert(tokens[3].value == "8080");
}

void testDoubleSlashCommentSkips() {
    const std::string  input = R"(
		server {
			// C++-style comment
			listen 8080;
		}
	)";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();
    assert(tokens[0].type == TokenType::KEYWORD_SERVER);
    assert(tokens[2].type == TokenType::KEYWORD_LISTEN);
    assert(tokens[3].value == "8080");
}

void testEscapedQuoteInString() {
    const std::string  input = R"(root "escaped \"quote\" inside";)";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();

    assert(tokens[0].type == TokenType::KEYWORD_ROOT);
    assert(tokens[1].type == TokenType::STRING);
    assert(tokens[1].value == "escaped \"quote\" inside");
}

void testEmptyStrings() {
    const std::string  input = R"(root ""; index '';)";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();

    assert(tokens[0].type == TokenType::KEYWORD_ROOT);
    assert(tokens[1].type == TokenType::STRING);
    assert(tokens[1].value == "");
    assert(tokens[3].type == TokenType::KEYWORD_INDEX);
    assert(tokens[4].type == TokenType::STRING);
    assert(tokens[4].value == "");
}

void testMixedWhitespace() {
    const std::string  input = "server\t{\r\n\tlisten 8080;\n\t}";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();

    assert(tokens[0].type == TokenType::KEYWORD_SERVER);
    assert(tokens[1].type == TokenType::LBRACE);
    assert(tokens[2].type == TokenType::KEYWORD_LISTEN);
    assert(tokens[3].type == TokenType::NUMBER);
    assert(tokens[4].type == TokenType::SEMICOLON);
    assert(tokens[5].type == TokenType::RBRACE);
}

void testPathLikeIdentifier() {
    const std::string  input = "location /.well-known/acme-challenge {}";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();

    assert(tokens[0].type == TokenType::KEYWORD_LOCATION);
    assert(tokens[1].type == TokenType::IDENTIFIER);
    assert(tokens[1].value == "/.well-known/acme-challenge");
}

void testUTF8BOMPrefix() {
    const std::string  input = "\xEF\xBB\xBFserver { listen 8080; }";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();

    assert(tokens[0].type == TokenType::KEYWORD_SERVER);
    assert(tokens[2].type == TokenType::KEYWORD_LISTEN);
}

void testEmptyInput() {
    Tokenizer          tokenizer("");
    std::vector<Token> tokens = tokenizer.tokenize();

    assert(tokens.size() == 1);
    assert(tokens[0].type == TokenType::END_OF_FILE);
}

void testOnlySemicolon() {
    Tokenizer          tokenizer(";");
    std::vector<Token> tokens = tokenizer.tokenize();

    assert(tokens.size() == 2);
    assert(tokens[0].type == TokenType::SEMICOLON);
    assert(tokens[1].type == TokenType::END_OF_FILE);
}

void testDuplicateSemicolons() {
    const std::string  input = "listen 8080;;";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();

    assert(tokens[0].type == TokenType::KEYWORD_LISTEN);
    assert(tokens[1].type == TokenType::NUMBER);
    assert(tokens[2].type == TokenType::SEMICOLON);
    assert(tokens[3].type == TokenType::SEMICOLON);
}

void testSymbolGarbage() {
    const std::string input = "@@ $$ %^&";
    try {
        Tokenizer tokenizer(input);
        (void) tokenizer.tokenize();
        assert(false && "Expected error for unexpected characters");
    } catch (const TokenizerError& e) {
        std::cerr << "✅ Caught symbol garbage: " << e.what() << '\n';
    }
}

void testMultilineQuotedString() {
    const std::string input = "root \"line1\nline2\";";
    try {
        Tokenizer tokenizer(input);
        (void) tokenizer.tokenize();
        assert(false && "Expected error for newline in quoted string");
    } catch (const TokenizerError& e) {
        std::cerr << "✅ Caught newline in quoted string: " << e.what() << '\n';
    }
}

void testSlashPath() {
    const std::string  input = "location / { index \"index.html\"; }";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();

    assert(tokens[0].type == TokenType::KEYWORD_LOCATION);
    assert(tokens[1].value == "/");
}

void testLongCommentThenDirective() {
    std::string        input = "/*" + std::string(8000, 'a') + "*/\nlisten 8080;";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();

    assert(tokens[0].type == TokenType::KEYWORD_LISTEN);
}

void testOneCharString() {
    const std::string  input = R"(index "a";)";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();

    assert(tokens[1].type == TokenType::STRING);
    assert(tokens[1].value == "a");
}

void testManySequentialTokens() {
    std::string        input = "listen 80; listen 81; listen 82;";
    Tokenizer          tokenizer(input);
    std::vector<Token> tokens = tokenizer.tokenize();

    assert(tokens.size() == 10); // 3x (listen + number + ;) + EOF
}

void testUtf8BOMDoesNotAffectTokens() {
    const std::string inputWithBOM = "\xEF\xBB\xBFserver { listen 8080; }";
    const std::string inputNoBOM   = "server { listen 8080; }";

    Tokenizer tokenizerBOM(inputWithBOM);
    Tokenizer tokenizerPlain(inputNoBOM);

    std::vector<Token> tokensBOM   = tokenizerBOM.tokenize();
    std::vector<Token> tokensPlain = tokenizerPlain.tokenize();

    assert(tokensBOM.size() == tokensPlain.size());

    for (std::size_t i = 0; i < tokensBOM.size(); ++i) {
        assert(tokensBOM[i].type == tokensPlain[i].type);
        assert(tokensBOM[i].value == tokensPlain[i].value);
        assert(tokensBOM[i].line == tokensPlain[i].line);
        assert(tokensBOM[i].column == tokensPlain[i].column);
    }
}

int main() {
    testDebugTokenType();
    testDebugToken();
    testSimpleTokenization();
    testUnterminatedStringThrows();
    testInvalidIdentifierThrows();
    testUnterminatedCommentThrows();
    testMultiLineCommentSkips();
    testValidEscapedString();
    testInvalidEscapeInSingleQuotedString();
    testInvalidDoubleEscapeSequence();
    testMaxStringLengthExceeded();
    testInvalidNumberSuffix();
    testHashCommentSkips();
    testDoubleSlashCommentSkips();
    testEscapedQuoteInString();
    testEmptyStrings();
    testMixedWhitespace();
    testPathLikeIdentifier();
    testUTF8BOMPrefix();
    testEmptyInput();
    testOnlySemicolon();
    testDuplicateSemicolons();
    testSymbolGarbage();
    testMultilineQuotedString();
    testSlashPath();
    testLongCommentThenDirective();
    testOneCharString();
    testManySequentialTokens();
    testUtf8BOMDoesNotAffectTokens();

    std::cout << "✅ All tokenizer tests passed.\n";
    return 0;
}
