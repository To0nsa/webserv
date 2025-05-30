# test/http/test_file_resolution.py

import http.client
import sys
import os
from urllib.parse import urlparse

SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")

def request(path, expected_status, expected_body_substr=None):
    parsed = urlparse(SERVER)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", path)
    res  = conn.getresponse()
    body = res.read().decode(errors="replace")
    conn.close()

    if res.status != expected_status:
        print(f"❌ GET {path} → {res.status} {res.reason} (expected {expected_status})")
        sys.exit(1)
    if expected_body_substr and expected_body_substr not in body:
        print(f"❌ GET {path} → body did not contain “{expected_body_substr}”")
        print("---- body ----")
        print(body)
        sys.exit(1)
    print(f"✅ GET {path} → {res.status} OK")
    return body

def run_tests():
    print("[ FILE RESOLUTION Test Suite ]")

    # 1) Serve static under /dir/ → test/data/dir/file.txt
    request("/dir/file.txt",           200, "This is a file inside /dir/")
    request("/dir/file.unknown",       200, "Binary? Nope—just text to test fallback")
    # percent-decoded name must work too
    request("/dir/file%2eunknown",     200, "Binary? Nope—just text to test fallback")

    # 2) Root-level static
    request("/index.html",             200, "<h1>Welcome to Webserv</h1>")
    request("/style.CsS",              200, "font-size")  # case-insensitive mapping
    request("/index.html.bak",         200, "backup copy")

    # 3) Directory-without-slash under /dir → redirect
    status_body = request("/dir",      301)  # Location: /dir/
    
    # 4) Non-existent file → 404
    request("/dir/not-there.txt",      404)

    # 5) Path-traversal is blocked
    request("/dir/../dir/file.txt",    403)
    request("/dir/../../secret.txt",   403)

if __name__ == "__main__":
    run_tests()
