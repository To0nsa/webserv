docker pull nginx

docker run --name my-nginx \
  -p 8080:8080 \
  -v /home/irychkov/Desktop/webserv_team/serverfiles/html:/usr/share/nginx/html \
  -v /home/irychkov/Desktop/webserv_team/configs/get_index.conf:/etc/nginx/conf.d/default.conf \
  -d nginx


docker stop my-nginx

docker start my-nginx

docker restart my-nginx

docker rm -f my-nginx

docker logs my-nginx

docker ps -a
