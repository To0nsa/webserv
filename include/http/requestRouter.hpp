/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestHandler.hpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/12 23:11:50 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/05 10:58:12 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

class Server;
class HttpRequest;
class HttpResponse;

HttpResponse handleRequest(const HttpRequest& request, const Server& server);
