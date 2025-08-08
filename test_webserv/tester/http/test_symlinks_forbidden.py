# test_webserv/http/test_symlinks_forbidden.py

import http.client
import os
import sys
from urllib.parse import urlparse

SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
UPLOAD_PATH = os.getenv("UPLOAD_DIR", os.path.join(SCRIPT_DIR, "../tester/data/upload_store"))
print(f"Upload path: {UPLOAD_PATH}")

def request(method, path, expected_status, body=None, headers=None):
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)

    if body and not headers:
        headers = {
            "Content-Type": "text/plain",
            "Content-Length": str(len(body))
        }

    conn.request(method, path, body=body, headers=headers or {})
    res = conn.getresponse()
    res.read()  # consume body
    conn.close()

    if res.status != expected_status:
        print(f"❌ {method} {path} → {res.status} {res.reason} (expected {expected_status})")
        sys.exit(1)
    print(f"✅ {method} {path} → {res.status} {res.reason}")

def setup_symlinks():
    real_path = os.path.join(UPLOAD_PATH, "real.txt")
    with open(real_path, "w") as f:
        f.write("test content")

    for name in ["link_get.txt", "link_post.txt", "link_delete.txt"]:
        link_path = os.path.join(UPLOAD_PATH, name)
        try:
            os.symlink(real_path, link_path)
        except FileExistsError:
            pass  # already exists

def cleanup():
    for name in ["real.txt", "link_get.txt", "link_post.txt", "link_delete.txt"]:
        path = os.path.join(UPLOAD_PATH, name)
        try:
            if os.path.exists(path) or os.path.islink(path):
                os.unlink(path)
        except FileNotFoundError:
            pass

def run_tests():
    print("[ SYMLINK FORBIDDEN Test Suite ]")
    setup_symlinks()

    request("POST",   "/upload_store/link_post.txt",   403, body="fake upload")
    request("DELETE", "/upload_store/link_delete.txt", 403)
    request("GET",    "/upload_store/link_get.txt",    403)

    cleanup()

if __name__ == "__main__":
    run_tests()
