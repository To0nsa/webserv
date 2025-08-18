/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 15:06:31 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/17 12:09:36 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "config/Config.hpp"              // for Config
#include "config/tokenizer/Tokenizer.hpp" // for Tokenizer
#include "config/tokenizer/token.hpp"     // for Token, TokenType (ptr only)
#include "core/Location.hpp"              // for Location
#include "core/Server.hpp"                // for Server
#include <cstddef>                        // for size_t
#include <initializer_list>               // for initializer_list
#include <span>                           // for span
#include <string>                         // for string
#include <vector>                         // for vector

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
    std::vector<std::string> collectArgs(std::span<const TokenType> validTypes);

    /////////////////////
    // --- Error Context
    std::string getLineSnippet() const;

  private:
    Tokenizer          _tokenizer; ///< Tokenizer instance used to produce the token stream.
    std::vector<Token> _tokens;    ///< Flattened list of tokens extracted from the source input.
    std::size_t        _pos = 0;   ///< Current index in the token stream.
};
