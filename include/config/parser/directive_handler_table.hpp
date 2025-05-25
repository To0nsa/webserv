/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   directive_handler_table.hpp                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/08 17:10:31 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/21 10:45:39 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Location.hpp"
#include "core/Server.hpp"

namespace directive {

using ServerHandler = std::function<void(Server& s, const std::vector<std::string>& args, int line,
                                         int column, const std::string& getLineSnippet)>;

using LocationHandler =
    std::function<void(Location& loc, const std::vector<std::string>& args, int line, int column,
                       const std::string& getLineSnippet)>;

const std::unordered_map<std::string, ServerHandler>&   serverHandlers();
const std::unordered_map<std::string, LocationHandler>& locationHandlers();

} // namespace directive