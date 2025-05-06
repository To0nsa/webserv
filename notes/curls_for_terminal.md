for i in {1..10}; do curl --no-keepalive 127.0.0.1:8080; done

for i in {1..10}; do curl 127.0.0.1:8080; done

for i in {1..10}; do curl --no-keepalive 127.0.0.1:8080 & done
wait


seq 1 10 | xargs -n 1 -P 10 curl --no-keepalive 127.0.0.1:8080


ab -n 100 -c 10 http://127.0.0.1:8080/
