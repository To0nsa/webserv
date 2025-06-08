#!/usr/bin/env python3
import http.client
import os
import shutil
import socket
import sys
import time
from urllib.parse import urlparse

# ─────────────────────────────────────────────────────────────────────────────
# Configuration
# ─────────────────────────────────────────────────────────────────────────────

HOST     = os.getenv("WEBSERV_HOST", "127.0.0.1")
PORT     = int(os.getenv("WEBSERV_PORT", "8080"))
BASE_URL = f"http://{HOST}:{PORT}"

# These directories/files live under your webroot (e.g. "test/data") so that
# GET /static/style.css maps to "test/data/static/style.css".
TEST_ROOT = "test/data"

# ─────────────────────────────────────────────────────────────────────────────
# Helpers
# ─────────────────────────────────────────────────────────────────────────────

def mkdir(path):
    """Create a directory if it does not exist."""
    os.makedirs(path, exist_ok=True)

def remove(path):
    """Remove file or directory if it exists."""
    if os.path.isdir(path):
        shutil.rmtree(path)
    elif os.path.exists(path):
        os.remove(path)

def create_file(path, size_bytes=None, content=b""):
    """
    Create a file at 'path'. If size_bytes is given, write that many zero-bytes.
    Otherwise, write 'content'.
    """
    dirpath = os.path.dirname(path)
    mkdir(dirpath)
    with open(path, "wb") as f:
        if size_bytes is not None:
            # Write size_bytes of zero-bytes
            chunk = b"\0" * 4096
            written = 0
            while written < size_bytes:
                to_write = min(4096, size_bytes - written)
                f.write(chunk[:to_write])
                written += to_write
        else:
            f.write(content)

def request(path, headers=None):
    """Simple GET via http.client."""
    parsed = urlparse(BASE_URL)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", path, headers=headers or {})
    res  = conn.getresponse()
    body = res.read()
    conn.close()
    return res.status, res.getheader("Content-Type"), body

# ─────────────────────────────────────────────────────────────────────────────
# Tests
# ─────────────────────────────────────────────────────────────────────────────

def test_content_type_sniffing():
    """
    1) For known extensions (.html, .css, .js, .png), verify Content-Type matches.  
    2) For unknown extension (.fooext), fallback to application/octet-stream.
    """
    print("[TEST] Content-Type sniffing...")

    # Prepare a few small files in TEST_ROOT/static for each known extension
    STATIC_DIR = os.path.join(TEST_ROOT, "static")
    remove(STATIC_DIR)
    mkdir(STATIC_DIR)

    files = {
        "index.html":       b"<html><body>HTML</body></html>",
        "style.css":        b"body { color: red; }",
        "script.js":        b"console.log('hello');",
        "image.PNG":        b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR",  # just a PNG header
        "binary.fooext":    b"\x00\x01\x02",                     # unknown extension
    }

    expected_mime = {
        ".html": "text/html",
        ".css":  "text/css",
        ".js":   "application/javascript",
        ".png":  "image/png",
        ".fooext": "application/octet-stream",  # unknown
    }

    # Create each file under TEST_ROOT/static/
    for filename, content in files.items():
        path = os.path.join(STATIC_DIR, filename)
        create_file(path, content=content)

    # Run requests and assert Content-Type
    for filename in files:
        url_path = f"/static/{filename}"
        status, ctype, _ = request(url_path)
        if status != 200:
            print(f"❌ GET {url_path} returned {status}, expected 200.")
            sys.exit(1)

        ext = os.path.splitext(filename)[1].lower()
        # Normalize ".PNG" → ".png", ".CsS" → ".css", etc.
        if ext not in expected_mime:
            ext = ext  # fall back to as‐is if not listed
        expected = expected_mime[ext]
        if ctype != expected:
            print(f"❌ GET {url_path} → Content-Type: {ctype!r} (expected {expected!r})")
            sys.exit(1)
        print(f"✅ {url_path} → Content-Type: {ctype}")

    remove(STATIC_DIR)
    print("→ Content-Type sniffing passed.\n")


def test_large_file_streaming():
    """
    1) Create a 5 MiB file under TEST_ROOT/streaming/large.bin  
    2) Open raw TCP socket, send GET /streaming/large.bin, and call recv(4096) repeatedly.  
    3) Strip off HTTP headers, then ensure total body‐bytes == file size and that we needed >1 recv() call.
    """
    print("[TEST] Large file streaming...")

    STREAM_DIR   = os.path.join(TEST_ROOT, "streaming")
    LARGE_PATH   = os.path.join(STREAM_DIR, "large.bin")
    remove(STREAM_DIR)
    mkdir(STREAM_DIR)

    FILE_SIZE = 5 * 1024 * 1024  # 5 MiB
    create_file(LARGE_PATH, size_bytes=FILE_SIZE)

    # Open a raw socket and send a minimal GET
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOST, PORT))
    request_line = (
        f"GET /streaming/large.bin HTTP/1.1\r\n"
        f"Host: {HOST}:{PORT}\r\n"
        "Connection: close\r\n"
        "\r\n"
    )
    sock.sendall(request_line.encode("ascii"))

    total_body_bytes = 0
    chunks = 0
    header_parsed = False
    buffer = b""

    while True:
        data = sock.recv(4096)
        if not data:
            break

        buffer += data

        # If we haven’t reached the end of the headers yet, look for “\r\n\r\n”
        if not header_parsed:
            idx = buffer.find(b"\r\n\r\n")
            if idx != -1:
                # Everything after idx+4 is the start of the body
                body_fragment = buffer[idx+4:]
                total_body_bytes += len(body_fragment)
                chunks += 1 if len(body_fragment) > 0 else 0
                header_parsed = True
                buffer = b""  # discard the header
            # else: we haven’t seen the full header yet; keep reading
        else:
            # We’re already in the body
            total_body_bytes += len(buffer)
            chunks += 1
            buffer = b""

        # Safety check: if we somehow accumulate more body‐bytes than FILE_SIZE
        if total_body_bytes > FILE_SIZE:
            print(f"❌ Received {total_body_bytes} body‐bytes (> {FILE_SIZE})—something is wrong.")
            sock.close()
            remove(STREAM_DIR)
            sys.exit(1)

    sock.close()

    if total_body_bytes != FILE_SIZE:
        print(f"❌ Large file streaming: received {total_body_bytes} bytes (expected {FILE_SIZE}).")
        remove(STREAM_DIR)
        sys.exit(1)

    if chunks <= 1:
        print(f"❌ Expected at least 2 recv() calls, got {chunks}.")
        remove(STREAM_DIR)
        sys.exit(1)

    print(f"✅ Large file streaming: {chunks} chunks, total {total_body_bytes} bytes.")
    remove(STREAM_DIR)
    print("→ Large file streaming passed.\n")


def test_empty_directory_autoindex():
    """
    1) Create an empty folder TEST_ROOT/emptydir/  
    2) GET /emptydir/ and verify HTML table present but no rows inside (no files),
       allowing exactly one ".." parent link if present.
    """
    print("[TEST] Empty directory autoindex...")

    EMPTY_DIR = os.path.join(TEST_ROOT, "emptydir")
    remove(EMPTY_DIR)
    mkdir(EMPTY_DIR)

    status, ctype, body = request("/emptydir/")
    if status != 200:
        print(f"❌ GET /emptydir/ returned {status} (expected 200).")
        remove(EMPTY_DIR)
        sys.exit(1)

    if not ctype.startswith("text/html"):
        print(f"❌ GET /emptydir/ → Content-Type: {ctype} (expected text/html).")
        remove(EMPTY_DIR)
        sys.exit(1)

    html = body.decode("utf-8", errors="replace")

    # Check that <table> exists
    if "<table" not in html or "</table>" not in html:
        print("❌ Autoindex page missing <table> structure for /emptydir/.")
        remove(EMPTY_DIR)
        sys.exit(1)

    # Count how many <tr> tags (header row + possibly parent link)
    row_count = html.count("<tr>")

    # Acceptable cases:
    #  1) row_count == 1: only header row (no entries at all)
    #  2) row_count == 2 and "../" in html: header + exactly one parent ("..") link
    if row_count == 1:
        # OK: purely empty, no parent link shown
        pass
    elif row_count == 2 and "../" in html:
        # OK: exactly one parent-directory entry
        pass
    else:
        print(f"❌ Expected only header (or header + ..), but found {row_count - 1} data rows.")
        remove(EMPTY_DIR)
        sys.exit(1)

    print("✅ Empty directory autoindex shows no files (only header row or header+..).")
    remove(EMPTY_DIR)
    print("→ Empty directory autoindex passed.\n")


# ─────────────────────────────────────────────────────────────────────────────
# Run all tests
# ─────────────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("\n=== Running MIME & Static File Tests ===\n")
    test_content_type_sniffing()
    test_large_file_streaming()
    test_empty_directory_autoindex()
