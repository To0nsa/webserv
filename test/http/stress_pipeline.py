#!/usr/bin/env python3
import os
import socket
import time
import random
import string
from urllib.parse import urlparse
import http.client

SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")
ENDPOINT = "/upload_store/pipe"
PIPELINE_DEPTH = 10
PIPELINE_COUNT = 50
BODY_SIZE = 4096

def generate_data(size=BODY_SIZE):
    return ''.join(random.choices(string.ascii_letters + string.digits, k=size))

def make_pipeline_block(i_base):
    block = ""
    expected = []
    paths = []
    for i in range(PIPELINE_DEPTH):
        uri = f"{ENDPOINT}{i_base + i}.txt"
        body = generate_data()
        paths.append(uri)
        block += (
            f"POST {uri} HTTP/1.1\r\n"
            f"Host: {urlparse(SERVER).hostname}\r\n"
            f"Content-Length: {len(body)}\r\n"
            f"Content-Type: text/plain\r\n"
            f"Connection: keep-alive\r\n\r\n"
            f"{body}"
            f"GET {uri} HTTP/1.1\r\n"
            f"Host: {urlparse(SERVER).hostname}\r\n"
            f"Connection: keep-alive\r\n\r\n"
            f"DELETE {uri} HTTP/1.1\r\n"
            f"Host: {urlparse(SERVER).hostname}\r\n"
            f"Connection: keep-alive\r\n\r\n"
        )
        expected.extend([201, 200, 200])
    return block, expected, paths

def parse_status_lines(raw):
    lines = raw.split("\r\n")
    return [int(line.split()[1]) for line in lines if line.startswith("HTTP/1.1")]

def run_pipeline_batch(batch_id):
    host, port = urlparse(SERVER).hostname, urlparse(SERVER).port
    try:
        s = socket.create_connection((host, port), timeout=5)
        s.settimeout(10)

        req_block, expected, paths = make_pipeline_block(batch_id * PIPELINE_DEPTH)
        s.sendall(req_block.encode())

        response = b""
        while True:
            try:
                part = s.recv(4096)
                if not part:
                    break
                response += part
            except socket.timeout:
                break
        s.close()

        actual = parse_status_lines(response.decode(errors="replace"))
        if actual != expected:
            print(f"[{batch_id}] ❌ Mismatch:\nExpected: {expected}\nActual:   {actual}")
            return False
        return True
    except Exception as e:
        print(f"[{batch_id}] ❌ Exception: {e}")
        return False

def cleanup_upload_store():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    print("[🧹] Cleaning up /upload_store...")
    deleted = 0
    for i in range(PIPELINE_COUNT * PIPELINE_DEPTH):
        path = f"{ENDPOINT}{i}.txt"
        conn.request("DELETE", path)
        res = conn.getresponse()
        res.read()
        if res.status == 200:
            deleted += 1
    conn.close()
    print(f"[DONE] Deleted {deleted} files.")

def pipeline_stress():
    print(f"\n[RUN] HTTP/1.1 pipelining test: {PIPELINE_COUNT} connections × {PIPELINE_DEPTH} ops = {PIPELINE_COUNT * PIPELINE_DEPTH * 3} requests")
    start = time.time()
    success = sum(run_pipeline_batch(i) for i in range(PIPELINE_COUNT))
    duration = time.time() - start
    rps = (PIPELINE_COUNT * PIPELINE_DEPTH * 3) / duration

    print(f"\n✅ {success}/{PIPELINE_COUNT} batches succeeded in {duration:.2f}s → {rps:.2f} RPS")
    cleanup_upload_store()
    if success == PIPELINE_COUNT:
        print("[PASS] stress_pipeline.py completed successfully")
    else:
        print("❗ Some pipeline batches failed")

if __name__ == "__main__":
    pipeline_stress()
