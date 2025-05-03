# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/04/29 14:10:33 by irychkov          #+#    #+#              #
#    Updated: 2025/05/01 21:00:37 by irychkov         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME = webserv
SRCS = main.cpp Config.cpp Server.cpp Location.cpp ConfigParser.cpp \
		SocketManager.cpp HttpRequest.cpp FilePath.cpp HandlePostUpload.cpp \
		HandleDelete.cpp RunCGI.cpp HttpResponse.cpp
OBJ = $(SRCS:.cpp=.o)
COMPILER = c++
CFLAGS = -Wall -Wextra -Werror -Wshadow -std=c++17

all: $(NAME)

$(NAME): $(OBJ)
	$(COMPILER) $(CFLAGS) -o $(NAME) $(OBJ)

%.o: %.cpp
	$(COMPILER) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
