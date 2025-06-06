/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   methodsHandler.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/05 10:46:53 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/05 17:22:09 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

class Server;
class Location;
class HttpRequest;
class HttpResponse;

HttpResponse handleGet(const HttpRequest&, const Server&, const Location&);
HttpResponse generateAutoindex(const std::string& filepath, const std::string& uri,
                               const HttpRequest& request, const Server& server);

HttpResponse handlePost(const HttpRequest&, const Server&, const Location&);
HttpResponse handleDelete(const HttpRequest&, const Server&, const Location&);
