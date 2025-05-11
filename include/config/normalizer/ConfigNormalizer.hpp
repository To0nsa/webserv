/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigNormalizer.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/11 21:02:21 by nlouis            #+#    #+#             */
/*   Updated: 2025/05/11 21:58:53 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include "config/Config.hpp"

class ConfigNormalizer {
  public:
    static void normalize(Config& config);
};
