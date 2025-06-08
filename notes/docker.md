docker pull nginx

docker run --name my-nginx \
  -p 8080:8080 \
  -v /home/nlouis/webserv/serverfiles/html:/usr/share/nginx/html \
  -v /home/nlouis/webserv/configs/get_index.conf:/etc/nginx/conf.d/default.conf \
  -d nginx


docker stop my-nginx

docker start my-nginx

docker restart my-nginx

docker rm -f my-nginx

docker logs my-nginx

docker ps -a

docker run --name my-nginx \
  -p 8080:8080 \
  -v /home/irychkov/Desktop/webserv/serverfiles/html:/usr/share/nginx/html \
  -v /home/irychkov/Desktop/webserv/configs/get_index.conf:/etc/nginx/conf.d/default.conf \
  -d nginx
