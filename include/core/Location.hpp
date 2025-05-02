/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Location.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 09:40:11 by irychkov          #+#    #+#             */
/*   Updated: 2025/04/30 14:19:58 by irychkov         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <string>
#include <vector>

struct Location {
		std::string path;
		std::vector<std::string> methods;
		std::string root;
		std::string index;
		bool autoindex;
		std::string redirect;
		int return_code;
		std::string upload_store;
		std::string cgi_extension;

		Location();
	};
