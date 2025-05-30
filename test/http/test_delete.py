import http.client
import os
import sys
import socket
import time
from urllib.parse import urlparse

SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")

def request(method, path, body=None, headers=None):
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request(method, path, body=body, headers=headers or {})
    res = conn.getresponse()
    data = res.read().decode(errors="replace")
    conn.close()
    return res.status, res.reason, data

def assert_status(method, path, expected_code, **kwargs):
    code, reason, _ = request(method, path, **kwargs)
    if code == expected_code:
        print(f"✅ {method} {path} → {code} {reason}")
    else:
        print(f"❌ {method} {path} → {code} {reason} (expected {expected_code})")
        sys.exit(1)

# ─────────────────────────────────────────────────────────────────────────────
# Helpers to isolate state
# ─────────────────────────────────────────────────────────────────────────────

def ensure_absent(path):
    """If the file exists, DELETE it; ignore 404."""
    code, _, _ = request("GET", path)
    if code == 200:
        request("DELETE", path)

def create_file(path, content="x"):
    """POST a small file, assert 201."""
    status, reason, _ = request("POST", path,
                                body=content,
                                headers={"Content-Type": "text/plain"})
    if status != 201:
        print(f"❌ setup POST {path} → {status} {reason} (expected 201)") 
        sys.exit(1)

# ─────────────────────────────────────────────────────────────────────────────
# Tests
# ─────────────────────────────────────────────────────────────────────────────

def test_file_deletion():
    test_path = "/upload_store/test_delete.txt"

    # -- setUp --
    ensure_absent(test_path)
    create_file(test_path, "hello world")

    # -- exercise & verify --
    status, reason, body = request("DELETE", test_path)
    if status == 200 and "<h1>File test_delete.txt deleted.</h1>" in body:
        print(f"✅ DELETE {test_path} → 200 {reason} with correct message")
    else:
        print(f"❌ DELETE {test_path} → {status} {reason}, body:\n{body!r}")
        sys.exit(1)

    assert_status("GET", test_path, 404)

    # -- tearDown --
    ensure_absent(test_path)


def test_delete_nonexistent():
    # nothing to set up
    assert_status("DELETE", "/doesnotexist", 404)


def test_delete_directory():
    # nothing to set up
    assert_status("DELETE", "/dir/", 403)
    _, _, body = request("DELETE", "/dir/")
    if "<h1>403 Forbidden</h1>" in body:
        print("✅ DELETE directory → custom 403 page served")
    else:
        print("❌ DELETE directory → custom 403 page not found")
        sys.exit(1)


def test_delete_path_traversal():
    assert_status("DELETE", "/../index.html", 403)
    assert_status("DELETE", "/dir/../../secret.txt", 403)


def test_delete_with_body():
    test_path = "/upload_store/test_delete_body.txt"

    # -- setUp --
    ensure_absent(test_path)
    create_file(test_path, "data")

    # -- exercise & verify --
    status, reason, _ = request("DELETE", test_path, body="ignored")
    if status == 200:
        print("✅ DELETE with body → 200 OK")
    else:
        print(f"❌ DELETE with body → {status} {reason} (expected 200)")
        sys.exit(1)

    # -- tearDown --
    ensure_absent(test_path)


def run_tests():
    print("[ DELETE Test Suite ]")
    test_file_deletion()
    test_delete_nonexistent()
    test_delete_directory()
    test_delete_path_traversal()
    test_delete_with_body()


if __name__ == "__main__":
    run_tests()
