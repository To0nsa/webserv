/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 10:34:50 by irychkov          #+#    #+#             */
/*   Updated: 2025/05/21 21:36:40 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "core/Server.hpp"
#include <vector>

class Config {
  public:
    ////////////////////////////////
    // --- Constructor
    Config()                             = default;
    ~Config()                            = default;
    Config(const Config&)                = default;
    Config& operator=(const Config&)     = default;
    Config(Config&&) noexcept            = default;
    Config& operator=(Config&&) noexcept = default;

    //////////////////
    // --- Public API
    void                       addServer(const Server& server);
    const std::vector<Server>& getServers() const;
    std::vector<Server>&       getServers();

  private:
    std::vector<Server> _servers; ///< List of all parsed servers
};
