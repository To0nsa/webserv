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


def setup_test_files():
    base_dir = "test/data/dir"
    os.makedirs(base_dir, exist_ok=True)
    with open(os.path.join(base_dir, "file.txt"), "w") as f:
        f.write("This is a file inside /dir/")
    with open(os.path.join(base_dir, "file.unknown"), "w") as f:
        f.write("Binary? Nope—just text to test fallback")
    with open("test/data/style.CsS", "w") as f:
        f.write("body { font-size: 14px; }")
    with open("test/data/index.html.bak", "w") as f:
        f.write("<!-- backup copy of index.html -->")

def cleanup_test_files():
    import shutil
    for f in ["style.CsS", "index.html.bak"]:
        try:
            os.remove(os.path.join("test/data", f))
        except FileNotFoundError:
            pass
    shutil.rmtree("test/data/dir", ignore_errors=True)


def run_tests():
    setup_test_files()
    print("[ FILE RESOLUTION Test Suite ]")

    # 1) Serve static under /dir/ → test/data/dir/file.txt
    request("/dir/file.txt",           200, "This is a file inside /dir/")
    request("/dir/file.unknown",       200, "Binary? Nope—just text to test fallback")
    # percent-decoded name must work too
    request("/dir/file%2eunknown",     200, "Binary? Nope—just text to test fallback")

    # 2) Root-level static
    request("/style.CsS",              200, "font-size")  # case-insensitive mapping
    request("/index.html.bak",         200, "backup copy")

    # 3) Directory-without-slash under /dir → redirect
    status_body = request("/dir",      301)  # Location: /dir/
    
    # 4) Non-existent file → 404
    request("/dir/not-there.txt",      404)

    # 5) Path-traversal is blocked
    request("/dir/../dir/file.txt",    403)
    request("/dir/../../secret.txt",   403)

    cleanup_test_files()

if __name__ == "__main__":
    run_tests()
