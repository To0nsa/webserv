/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 15:06:31 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/21 11:07:01 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "config/Config.hpp"
#include "config/parser/ConfigParseError.hpp"
#include "config/tokenizer/Tokenizer.hpp"
#include "config/tokenizer/token.hpp"

#include <set>
#include <string>
#include <vector>

class ConfigParser {
  public:
    //////////////////
    // --- Public API
    ConfigParser(std::string source);
    Config parseConfig();

  private:
    ////////////////////////////
    // --- Server Block Parsing
    Server parseServer();
    void   parseServerDirective(Server& server);

    Location parseLocation();
    void     parseLocationDirective(Location& location);

    ////////////////////////
    // --- Token Navigation

    const Token& current() const;
    const Token& previous() const;
    const Token& peek(std::size_t offset = 1) const;
    const Token& lookBehind(std::size_t offset = 1) const;
    const Token& advance();
    bool         isAtEnd() const;
    bool         match(TokenType type);
    void         expect(TokenType expected, const std::string& context);
    Token        expectOneOf(std::initializer_list<TokenType> types, const std::string& context);
    std::vector<std::string> collectArgs(std::initializer_list<TokenType> validTypes);

    /////////////////////
    // --- Error Context
    std::string getLineSnippet() const;

  private:
    Tokenizer          _tokenizer; ///< Tokenizer instance used to produce the token stream.
    std::vector<Token> _tokens;    ///< Flattened list of tokens extracted from the source input.
    std::size_t        _pos = 0;   ///< Current index in the token stream.
};
