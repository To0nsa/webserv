/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 10:34:50 by irychkov          #+#    #+#             */
/*   Updated: 2025/08/18 19:43:23 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    Config.hpp
 * @brief   Declares the Config aggregate for parsed virtual servers.
 *
 * @details Owns the collection of @ref Server objects produced by the parser
 *          and subsequently normalized/validated before runtime use.
 *
 * @ingroup config
 */

#pragma once

#include "core/Server.hpp"
#include <vector>

/**
 * @brief Top-level configuration aggregate.
 *
 * @details Simple container holding all parsed @ref Server instances from the
 *          configuration input. Acts as the handoff object between parsing and
 *          later normalization/validation stages.
 *
 * @ingroup config
 */
class Config {
  public:
    //=== Construction & Special Members =====================================

    /** @name Construction & special members */
    ///@{
    Config()                             = default;
    ~Config()                            = default;
    Config(const Config&)                = default;
    Config& operator=(const Config&)     = default;
    Config(Config&&) noexcept            = default;
    Config& operator=(Config&&) noexcept = default;
    ///@}

    //=== Public API ==========================================================

    /** @name Public API */
    ///@{
    void                       addServer(const Server& server);
    const std::vector<Server>& getServers() const;
    std::vector<Server>&       getServers();
    ///@}

  private:
    std::vector<Server> _servers; ///< List of all parsed servers.
};
