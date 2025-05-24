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
