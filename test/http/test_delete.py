import http.client
import os
import sys
from urllib.parse import urlparse

SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")

def log_request(method, path, status, reason, expected=None):
    if expected is not None and status != expected:
        print(f"❌ {method} {path} → {status} {reason} (expected {expected})")
        sys.exit(1)
    tag = "✅" if method == "DELETE" and status == 200 else "[LOG]"
    print(f"{tag} {method} {path} → {status} {reason}")

def request(method, path, body=None, headers=None, expected=None):
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request(method, path, body=body, headers=headers or {})
    res = conn.getresponse()
    data = res.read().decode(errors="replace")
    conn.close()
    log_request(method, path, res.status, res.reason, expected)
    return res.status, res.reason, data

# ─────────────────────────────────────────────────────────────────────────────
# Helpers
# ─────────────────────────────────────────────────────────────────────────────

def ensure_absent(path):
    status, _, _ = request("GET", path)
    if status == 200:
        request("DELETE", path, expected=200)

def create_file(path, content="x"):
    request("POST", path, body=content,
            headers={"Content-Type": "text/plain"}, expected=201)

# ─────────────────────────────────────────────────────────────────────────────
# Tests
# ─────────────────────────────────────────────────────────────────────────────

def test_file_deletion():
    test_path = "/upload_store/test_delete.txt"
    ensure_absent(test_path)
    create_file(test_path, "hello world")

    status, reason, body = request("DELETE", test_path)
    if status != 200 or "<h1>File test_delete.txt deleted.</h1>" not in body:
        print(f"❌ DELETE {test_path} → {status} {reason}")
        sys.exit(1)

    request("GET", test_path, expected=404)

def test_delete_nonexistent():
    request("DELETE", "/doesnotexist", expected=404)

def test_delete_directory():
    request("DELETE", "/dir/", expected=403)

def test_delete_path_traversal():
    request("DELETE", "/../index.html", expected=403)
    request("DELETE", "/dir/../../secret.txt", expected=403)

def test_delete_with_body():
    test_path = "/upload_store/test_delete_body.txt"
    ensure_absent(test_path)
    create_file(test_path, "data")
    request("DELETE", test_path, body="ignored", expected=200)

# ─────────────────────────────────────────────────────────────────────────────
# Run
# ─────────────────────────────────────────────────────────────────────────────

def run_tests():
    print("[ DELETE Test Suite ]")
    test_file_deletion()
    test_delete_nonexistent()
    test_delete_directory()
    test_delete_path_traversal()
    test_delete_with_body()

if __name__ == "__main__":
    run_tests()
