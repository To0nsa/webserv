for i in {1..10}; do curl --no-keepalive 127.0.0.1:8080; done

for i in {1..10}; do curl 127.0.0.1:8080; done

for i in {1..10}; do curl --no-keepalive 127.0.0.1:8080 & done
wait


seq 1 10 | xargs -n 1 -P 10 curl --no-keepalive 127.0.0.1:8080


ab -n 100 -c 10 http://127.0.0.1:8080/


nc 127.0.0.1 8080
GET / HTTP/1.1


nc 127.0.0.1 8080
GET / HTTP/1.1\r


printf "GET / HTTP/1.1\r\n\r\n" | nc 127.0.0.1 8080


printf "G\r\n" | nc 127.0.0.1 8080

echo -ne 'GET / HTTP/1.1\r\n\r\n' | nc 127.0.0.1 8080

echo -ne 'GET / HTTP/1.0\r\n\r\n' | nc 127.0.0.1 8080


curl -X POST http://localhost:8001/uploads/ACTIONPLAN.md \
     --data-binary "@/home/irychkov/Desktop/webserv_team/ACTIONPLAN.md"

echo -ne 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8001

echo -ne 'GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n' | nc 127.0.0.1 8001



echo -ne 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080

echo -ne 'GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n' | nc 127.0.0.1 8080


irychkov@irychkov42:~/Desktop/webserv/serverfiles/cgi-bin$ SCRIPT_NAME=/directory/fake.bla \
PATH_INFO=/directory/fake.bla \
REQUEST_METHOD=GET \
QUERY_STRING= \
CONTENT_LENGTH=0 \
CONTENT_TYPE=text/plain \
SERVER_PROTOCOL=HTTP/1.1 \
GATEWAY_INTERFACE=CGI/1.1 \
SERVER_SOFTWARE=webserv/1.0 \
DOCUMENT_ROOT=/home/irychkov/Desktop/webserv/serverfiles/html/YoupiBanane \
SERVER_NAME=localhost \
SERVER_PORT=8080 \
PATH_TRANSLATED=/home/irychkov/Desktop/webserv/serverfiles/html/YoupiBanane/fake.bla \
REMOTE_ADDR=127.0.0.1 \
REQUEST_URI=/directory/fake.bla \
SCRIPT_FILENAME=/home/irychkov/Desktop/webserv/serverfiles/html/YoupiBanane/fake.bla \
/home/irychkov/Desktop/webserv/serverfiles/cgi-bin/ubuntu_cgi_tester < /dev/null


echo -n "name=test&value=123" | \
SCRIPT_NAME=/directory/fake.bla \
PATH_INFO=/directory/fake.bla \
REQUEST_METHOD=POST \
QUERY_STRING= \
CONTENT_LENGTH=21 \
CONTENT_TYPE=application/x-www-form-urlencoded \
SERVER_PROTOCOL=HTTP/1.1 \
GATEWAY_INTERFACE=CGI/1.1 \
SERVER_SOFTWARE=webserv/1.0 \
DOCUMENT_ROOT=/home/irychkov/Desktop/webserv/serverfiles/html/YoupiBanane \
SERVER_NAME=localhost \
SERVER_PORT=8080 \
PATH_TRANSLATED=/home/irychkov/Desktop/webserv/serverfiles/html/YoupiBanane/fake.bla \
REMOTE_ADDR=127.0.0.1 \
REQUEST_URI=/directory/fake.bla \
SCRIPT_FILENAME=/home/irychkov/Desktop/webserv/serverfiles/html/YoupiBanane/fake.bla \
REDIRECT_STATUS=200 \
/home/irychkov/Desktop/webserv/serverfiles/cgi-bin/ubuntu_cgi_tester
