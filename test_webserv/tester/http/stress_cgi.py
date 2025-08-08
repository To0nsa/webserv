#!/usr/bin/env python3
import os
import sys
import time
import random
import string
import requests
from concurrent.futures import ThreadPoolExecutor, as_completed

SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")
CGI_PATHS = [
    "/cgi-bin/hello.sh",
    "/cgi-bin/hello.py",
    "/cgi-bin/CgiEnv.py"
]
CONCURRENCY = int(os.getenv("STRESS_CONCURRENCY", 40))
REQUESTS_PER_THREAD = int(os.getenv("STRESS_REQS_PER_THREAD", 20))
TIMEOUT = 5  # seconds
HEADERS = {"Content-Type": "text/plain"}  # for POST requests

def generate_body(size=128):
    return ''.join(random.choices(string.ascii_letters + string.digits, k=size))

def test_cgi_request(index):
    path = random.choice(CGI_PATHS)
    method = random.choice(["GET", "POST"])
    url = SERVER + path
    try:
        if method == "GET":
            resp = requests.get(url, timeout=TIMEOUT)
        else:
            resp = requests.post(url, data=generate_body(), headers=HEADERS, timeout=TIMEOUT)
        if resp.status_code != 200:
            return index, f"{method} {path} → HTTP {resp.status_code}"
        text = resp.text
        # Basic content validation
        if path.endswith("hello.sh") and "Hello from Bash CGI" not in text:
            return index, f"{method} {path} → bad body"
        if path.endswith("hello.py") and "Hello from CGI" not in text:
            return index, f"{method} {path} → bad body"
        if path.endswith("CgiEnv.py") and f"REQUEST_METHOD = {method}" not in text:
            return index, f"{method} {path} → wrong method in env"
        return index, "OK"
    except Exception as e:
        return index, f"{method} {path} → ERR: {e}"

def stress_test():
    total = CONCURRENCY * REQUESTS_PER_THREAD
    print(f"Starting CGI stress test: {CONCURRENCY} threads × {REQUESTS_PER_THREAD} requests each ({total} total)...")
    start = time.time()
    success = failure = 0

    with ThreadPoolExecutor(max_workers=CONCURRENCY) as executor:
        futures = [
            executor.submit(test_cgi_request, tid * REQUESTS_PER_THREAD + i)
            for tid in range(CONCURRENCY)
            for i in range(REQUESTS_PER_THREAD)
        ]
        for future in as_completed(futures):
            idx, result = future.result()
            if result == "OK":
                success += 1
            else:
                failure += 1
                print(f"[{idx}] ❌ {result}")

    duration = time.time() - start
    rps = total / duration if duration > 0 else float("inf")
    print(f"\n✅ Finished: {success}/{total} succeeded in {duration:.2f}s ({rps:.2f} RPS)")
    if failure > 0:
        print(f"❗ {failure} failed requests")
        sys.exit(1)

if __name__ == "__main__":
    stress_test()
