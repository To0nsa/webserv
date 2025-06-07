/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   urlUtils.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/06 13:11:40 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/06 13:26:29 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <string>
#include <unordered_map>

std::string                                  decodePercentEncoding(const std::string& encoded);
std::string                                  percentDecodeForm(const std::string& input);
std::unordered_map<std::string, std::string> parseFormUrlEncoded(const std::string& body);