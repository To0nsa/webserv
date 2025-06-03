#!/usr/bin/env python3
import os
import socket
import time
import random
import string
import re
from urllib.parse import urlparse
import http.client
from concurrent.futures import ThreadPoolExecutor

# ─── CONFIGURATION ─────────────────────────────────────────────────────────────

SERVER       = os.getenv("WEBSERV_URL", "http://localhost:8080")
ENDPOINT     = "/upload_store/pipe"
PIPELINE_DEPTH = 10      # 10 (POST→GET→DELETE) groups per batch
PIPELINE_COUNT = 50      # 50 batches total
BODY_SIZE      = 4096    # each POST’s body is 4096 bytes
MAX_WORKERS    = 1       # set to >1 to run batches in parallel

# ─── HELPERS ────────────────────────────────────────────────────────────────────

def generate_data(size=BODY_SIZE):
    """Return a random ASCII string of exactly `size` bytes."""
    return "".join(random.choices(string.ascii_letters + string.digits, k=size))

def parse_status_lines(raw_text):
    """
    Use a regex to find every occurrence of “HTTP/1.1 <XXX>” 
    and return the list of integer status codes.
    """
    return [int(m.group(1)) for m in re.finditer(r"HTTP/1\.1 (\d{3})", raw_text)]

def make_pipeline_block(i_base):
    """
    Build one block containing PIPELINE_DEPTH × (POST→GET→DELETE),
    and return (pipelined_bytes, expected_status_list).
    """
    block_parts = []
    expected = []

    parsed = urlparse(SERVER)
    host_header = parsed.hostname
    if parsed.port:
        host_header += f":{parsed.port}"

    for i in range(i_base, i_base + PIPELINE_DEPTH):
        path = f"{ENDPOINT}{i}.txt"

        # 1) POST
        body = generate_data()
        post_headers = (
            f"POST {path} HTTP/1.1\r\n"
            f"Host: {host_header}\r\n"
            "Content-Type: text/plain\r\n"
            f"Content-Length: {len(body)}\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
        )
        block_parts.append(post_headers.encode("ascii") + body.encode("ascii"))
        expected.append(201)

        # 2) GET
        get_req = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {host_header}\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
        )
        block_parts.append(get_req.encode("ascii"))
        expected.append(200)

        # 3) DELETE
        delete_req = (
            f"DELETE {path} HTTP/1.1\r\n"
            f"Host: {host_header}\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
        )
        block_parts.append(delete_req.encode("ascii"))
        expected.append(200)

    return b"".join(block_parts), expected

def run_pipeline_batch(batch_id):
    """
    Send one pipelined batch (10×(POST, GET, DELETE)) and read back
    until we’ve seen all 30 status lines (or the server closes).
    """
    try:
        parsed = urlparse(SERVER)
        s = socket.create_connection((parsed.hostname, parsed.port))
        # To be safe, give the socket a small receive timeout (in seconds)
        s.settimeout(5.0)

        block, expected = make_pipeline_block(batch_id * PIPELINE_DEPTH)
        s.sendall(block)

        response = b""
        expected_count = len(expected)  # should be 30

        start = time.time()
        while True:
            try:
                part = s.recv(4096)
            except socket.timeout:
                # No new data for 5 seconds → assume server is done sending
                break

            if not part:
                # Server closed connection
                break

            response += part
            decoded = response.decode("ascii", errors="replace")
            # As soon as we see >= 30 matches of “HTTP/1.1 <status>”
            found = parse_status_lines(decoded)
            if len(found) >= expected_count:
                break

            # loop again until all 30 appear or timeout/EOF

        s.close()

        actual = parse_status_lines(decoded)
        if actual != expected:
            snippet = decoded[:1024]
            print(f"[Batch {batch_id}] ❌ Mismatch:")
            print(f"  Expected status sequence: {expected}")
            print(f"  Actual   status sequence: {actual}")
            print("  --- Response snippet (first 1 KiB) ---")
            print(snippet + ("..." if len(decoded) > 1024 else ""))
            print("  ---------------------------------------\n")
            return False

        return True

    except Exception as e:
        print(f"[Batch {batch_id}] ❌ Exception during pipelining: {e}")
        return False

def cleanup_upload_store():
    """
    After all batches, delete every file under /upload_store/.
    """
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    print("[🧹] Cleaning up /upload_store/ ...")
    deleted = 0
    total_to_delete = PIPELINE_COUNT * PIPELINE_DEPTH
    for i in range(total_to_delete):
        path = f"{ENDPOINT}{i}.txt"
        conn.request("DELETE", path, headers={"Host": parsed.hostname})
        resp = conn.getresponse()
        # 200 (deleted) or 404 (not present) are fine
        if resp.status in (200, 404):
            deleted += 1
        resp.read()
    conn.close()
    print(f"[🧹] Deleted {deleted}/{total_to_delete} entries.\n")

# ─── MAIN STRESS FUNCTION ────────────────────────────────────────────────────────

def pipeline_stress():
    print(f"➡️  Starting stress test: {PIPELINE_COUNT} batches × depth {PIPELINE_DEPTH} ...")
    start_time = time.time()

    if MAX_WORKERS > 1:
        with ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
            futures = [executor.submit(run_pipeline_batch, i) for i in range(PIPELINE_COUNT)]
            results = [f.result() for f in futures]
    else:
        results = [run_pipeline_batch(i) for i in range(PIPELINE_COUNT)]

    duration = time.time() - start_time
    total_requests = PIPELINE_COUNT * PIPELINE_DEPTH * 3
    rps = total_requests / duration if duration > 0 else float("inf")
    success_count = sum(1 for ok in results if ok)

    print(f"\n✅ {success_count}/{PIPELINE_COUNT} batches succeeded in {duration:.2f}s → {rps:.2f} RPS\n")
    cleanup_upload_store()

    if success_count == PIPELINE_COUNT:
        print("[PASS] stress_pipeline.py completed successfully")
    else:
        print("❗ Some pipeline batches FAILED")

# ─── ENTRY POINT ─────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    pipeline_stress()
