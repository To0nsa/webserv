/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/05 15:06:31 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/18 19:43:48 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    ConfigParser.hpp
 * @brief   Declares the ConfigParser for syntactic/semantic directive parsing.
 *
 * @details Consumes a token stream from @ref Tokenizer and builds a concrete
 *          configuration model (@ref Config, @ref Server, @ref Location) by
 *          dispatching recognized directives to handler tables.
 *
 * @ingroup config_parsing
 */

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

/**
 * @brief Syntactic parser for the webserv configuration language.
 *
 * @details Validates block structure (`server` → `location`) and delegates
 *          directive semantics to the handler tables. Produces a fully
 *          constructed @ref Config tree or throws on errors.
 *
 * @ingroup config_parsing
 */
class ConfigParser {
  public:
    //=== Public API ==========================================================
    /** @name Public API */
    ///@{
    ConfigParser(std::string source);
    Config parseConfig();
    ///@}

  private:
    //=== Server Block Parsing ===============================================
    /** @name Server block parsing */
    ///@{
    Server parseServer();
    void   parseServerDirective(Server& server);

    Location parseLocation();
    void     parseLocationDirective(Location& location);
    ///@}

    //=== Token Navigation ====================================================
    /** @name Token navigation */
    ///@{
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
    ///@}

    //=== Error Context =======================================================
    /** @name Error context */
    ///@{
    std::string getLineSnippet() const;
    ///@}

  private:
    //=== Internal State ======================================================
    /** @name Internal state */
    ///@{
    Tokenizer          _tokenizer; ///< Produces the token stream from the raw source.
    std::vector<Token> _tokens;    ///< Flattened token list consumed by the parser.
    std::size_t        _pos = 0;   ///< Current index in the token stream.
    ///@}
};
