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

echo -ne 'GET /cgi-bin/hang.py HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n' | nc 127.0.0.1 8001

echo -ne 'GET /cgi-bin/hang.py HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\nGET /cgi-bin/hello.py HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\nGET /cgi-bin/hello.py HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n' | nc 127.0.0.1 8001

echo -ne 'GET /cgi-bin/hello.py HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\nGET /cgi-bin/hello.py HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\nGET /cgi-bin/hello.py HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n' | nc 127.0.0.1 8001


echo -ne 'GET /cgi-bin/hello.py HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\nPOST /upload_store/hello.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 20\r\nConnection: keep-alive\r\n\r\nHELLO'| nc 127.0.0.1 8001


echo -ne 'POST /upload_store/hello.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 20\r\nContent-Type: text/plain\r\nConnection: keep-alive\r\n\r\nGET /cgi-bin/hello.py HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n'| nc 127.0.0.1 8001


echo -ne 'POST /upload_store/hello.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 20\r\nConnection: keep-alive\r\n\r\nGET /cgi-bin/hello.py HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n'| nc 127.0.0.1 8001

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

ab -n 1000 -c 100 http://localhost:8001/index.html
This is ApacheBench, Version 2.3 <$Revision: 1903618 $>
Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/
Licensed to The Apache Software Foundation, http://www.apache.org/

Benchmarking localhost (be patient)
Completed 100 requests
Completed 200 requests
Completed 300 requests
Completed 400 requests
Completed 500 requests
Completed 600 requests
Completed 700 requests
Completed 800 requests
Completed 900 requests
Completed 1000 requests
Finished 1000 requests


Server Software:        
Server Hostname:        localhost
Server Port:            8001

Document Path:          /index.html
Document Length:        244 bytes

Concurrency Level:      100
Time taken for tests:   0.386 seconds
Complete requests:      1000
Failed requests:        0
Total transferred:      328000 bytes
HTML transferred:       244000 bytes
Requests per second:    2588.51 [#/sec] (mean)
Time per request:       38.632 [ms] (mean)
Time per request:       0.386 [ms] (mean, across all concurrent requests)
Transfer rate:          829.13 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    1   1.5      0       6
Processing:     7   36   7.8     36      59
Waiting:        1   36   7.7     36      58
Total:          7   37   8.4     36      61

Percentage of the requests served within a certain time (ms)
  50%     36
  66%     37
  75%     37
  80%     38
  90%     44
  95%     58
  98%     60
  99%     61
 100%     61 (longest request)


ab -n 10000 -c 200 http://localhost:8001/index.html
This is ApacheBench, Version 2.3 <$Revision: 1903618 $>
Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/
Licensed to The Apache Software Foundation, http://www.apache.org/

Benchmarking localhost (be patient)
Completed 1000 requests
Completed 2000 requests
Completed 3000 requests
Completed 4000 requests
Completed 5000 requests
Completed 6000 requests
Completed 7000 requests
Completed 8000 requests
Completed 9000 requests
Completed 10000 requests
Finished 10000 requests


Server Software:        
Server Hostname:        localhost
Server Port:            8001

Document Path:          /index.html
Document Length:        244 bytes

Concurrency Level:      200
Time taken for tests:   3.503 seconds
Complete requests:      10000
Failed requests:        0
Total transferred:      3280000 bytes
HTML transferred:       2440000 bytes
Requests per second:    2855.09 [#/sec] (mean)
Time per request:       70.050 [ms] (mean)
Time per request:       0.350 [ms] (mean, across all concurrent requests)
Transfer rate:          914.52 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   1.7      0      19
Processing:    10   69   5.9     69      77
Waiting:        1   69   6.0     69      77
Total:         21   69   5.3     69      82

Percentage of the requests served within a certain time (ms)
  50%     69
  66%     71
  75%     73
  80%     74
  90%     75
  95%     75
  98%     76
  99%     77
 100%     82 (longest request)

throw std::bad_alloc(); // Simulate memory allocation failure for testing
