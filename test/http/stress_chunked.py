import os
import random
import string
import socket
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from urllib.parse import urlparse

SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")
ENDPOINT = "/upload_store/chunked"
TOTAL = 200  # total requests
THREADS = 50  # concurrent workers
CHUNK_SIZE = 8192  # per chunk (8 KiB)
CHUNK_COUNT = 8  # → total ~64 KiB per file

def make_chunked_body():
    chunks = []
    for _ in range(CHUNK_COUNT):
        data = ''.join(random.choices(string.ascii_letters + string.digits, k=CHUNK_SIZE))
        chunks.append(f"{len(data):X}\r\n{data}\r\n")
    chunks.append("0\r\n\r\n")
    return ''.join(chunks)

def send_chunked_post(index):
    parsed = urlparse(SERVER)
    host = parsed.hostname
    port = parsed.port
    uri = f"{ENDPOINT}{index}.txt"
    path = f"POST {uri} HTTP/1.1\r\nHost: {host}\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n"
    body = make_chunked_body()

    try:
        sock = socket.create_connection((host, port), timeout=5)
        sock.sendall((path + body).encode())
        response = sock.recv(1024).decode(errors="replace")
        sock.close()
        if "201" in response:
            return uri, True
        return uri, False
    except Exception:
        return uri, False

def delete_uploaded_file(uri):
    try:
        parsed = urlparse(SERVER)
        import http.client
        conn = http.client.HTTPConnection(parsed.hostname, parsed.port, timeout=3)
        conn.request("DELETE", uri)
        res = conn.getresponse()
        conn.close()
        return res.status == 200
    except:
        return False

def chunked_stress():
    print(f"\n[RUN] Chunked encoding stress: {TOTAL} uploads with {THREADS} threads")
    start = time.time()

    uploaded = []

    with ThreadPoolExecutor(max_workers=THREADS) as executor:
        futures = [executor.submit(send_chunked_post, i) for i in range(TOTAL)]
        for fut in as_completed(futures):
            uri, ok = fut.result()
            if ok:
                uploaded.append(uri)
            else:
                print(f"❌ Failed POST → {uri}")

    duration = time.time() - start
    print(f"\n✅ POSTed {len(uploaded)}/{TOTAL} files via chunked encoding in {duration:.2f}s")

    print("[🧹] Deleting uploaded files...")
    deleted = 0
    for uri in uploaded:
        if delete_uploaded_file(uri):
            deleted += 1

    print(f"[DONE] Deleted {deleted}/{len(uploaded)} files\n")
    if len(uploaded) == deleted:
        print("[PASS] stress_chunked.py completed successfully")
    else:
        print("[WARN] Some files not deleted")

if __name__ == "__main__":
    chunked_stress()
