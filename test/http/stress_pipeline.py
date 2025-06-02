#!/usr/bin/env python3
import os
import socket
import time
import random
import string
from urllib.parse import urlparse
import http.client
from concurrent.futures import ThreadPoolExecutor

# ─── CONFIGURATION ─────────────────────────────────────────────────────────────

# e.g., "http://localhost:8080" or export WEBSERV_URL in your environment
SERVER     = os.getenv("WEBSERV_URL", "http://localhost:8080")
ENDPOINT   = "/upload_store/pipe"
PIPELINE_DEPTH = 10      # how many (POST→GET→DELETE) cycles per batch
PIPELINE_COUNT = 50      # how many batches to issue
BODY_SIZE      = 4096    # size of each POST payload in bytes

# Set to >1 if you want to run batches in parallel (adjust to your CPU/IO capacity)
MAX_WORKERS    = 1

# ─── HELPERS ────────────────────────────────────────────────────────────────────

def generate_data(size=BODY_SIZE):
    """Return a random ASCII string of exactly `size` bytes."""
    # Use letters and digits to avoid any binary/encoding surprises.
    return "".join(random.choices(string.ascii_letters + string.digits, k=size))

def parse_status_lines(raw_text):
    """
    Extract all status codes from lines that start with "HTTP/1.1".
    Returns a list of integers, e.g. [201, 200, 200, ...].
    """
    codes = []
    for line in raw_text.split("\r\n"):
        if line.startswith("HTTP/1.1"):
            parts = line.split()
            if len(parts) >= 2 and parts[1].isdigit():
                codes.append(int(parts[1]))
    return codes

def make_pipeline_block(i_base):
    """
    Build a single pipeline block of length PIPELINE_DEPTH:
      [POST /upload_store/pipe{i}.txt → GET /upload_store/pipe{i}.txt → DELETE /upload_store/pipe{i}.txt]
    for i in [i_base, i_base+1, ..., i_base + PIPELINE_DEPTH - 1].
    Returns (concatenated_bytes, expected_status_list).
    """
    block_parts = []
    expected = []

    parsed = urlparse(SERVER)
    host_header = parsed.hostname
    if parsed.port:
        host_header += f":{parsed.port}"

    for i in range(i_base, i_base + PIPELINE_DEPTH):
        path = f"{ENDPOINT}{i}.txt"

        # 1. POST
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

        # 2. GET
        get_req = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {host_header}\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
        )
        block_parts.append(get_req.encode("ascii"))
        expected.append(200)

        # 3. DELETE
        delete_req = (
            f"DELETE {path} HTTP/1.1\r\n"
            f"Host: {host_header}\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
        )
        block_parts.append(delete_req.encode("ascii"))
        expected.append(200)

    # Concatenate all sub‐requests into one pipelined buffer:
    return b"".join(block_parts), expected

def run_pipeline_batch(batch_id):
    """
    Open a single TCP connection, send PIPELINE_DEPTH×3 concatenated requests,
    read until we’ve parsed all expected status lines, then verify codes.
    Returns True if expected == actual; False otherwise.
    """
    try:
        s = socket.create_connection((urlparse(SERVER).hostname, urlparse(SERVER).port))
        block, expected = make_pipeline_block(batch_id * PIPELINE_DEPTH)
        s.sendall(block)

        # Read until we've seen all status lines (len(expected) total)
        response = b""
        expected_count = len(expected)

        while True:
            part = s.recv(4096)
            if not part:
                # Socket closed by server (EOF), stop reading
                break
            response += part
            # As soon as we have >= expected_count status lines, we can stop
            decoded = response.decode("ascii", errors="replace")
            if len(parse_status_lines(decoded)) >= expected_count:
                break

        s.close()

        actual = parse_status_lines(decoded)
        if actual != expected:
            # On mismatch, dump the first 1 KiB of raw response for diagnostics
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
    After all batches, delete every file under /upload_store/ (0.txt … up to PIPELINE_COUNT*PIPELINE_DEPTH-1.txt).
    Uses a single HTTPConnection and issues sequential DELETEs.
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
        # We expect either 200 (deleted) or 404 (already not present)
        if resp.status in (200, 404):
            deleted += 1
        resp.read()  # drain
    conn.close()
    print(f"[🧹] Deleted {deleted}/{total_to_delete} entries.\n")

# ─── MAIN STRESS FUNCTION ────────────────────────────────────────────────────────

def pipeline_stress():
    """
    Run PIPELINE_COUNT batches of PIPELINE_DEPTH pipelined requests each,
    optionally in parallel with MAX_WORKERS.
    """
    print(f"➡️  Starting stress test: {PIPELINE_COUNT} batches × depth {PIPELINE_DEPTH} ...")
    start_time = time.time()

    if MAX_WORKERS > 1:
        # Run batches in parallel
        with ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
            futures = [executor.submit(run_pipeline_batch, i) for i in range(PIPELINE_COUNT)]
            results = [f.result() for f in futures]
    else:
        # Sequential execution
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
