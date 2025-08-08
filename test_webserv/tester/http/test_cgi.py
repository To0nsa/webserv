#!/usr/bin/env python3
"""
General CGI functionality tests for Webserv with GET, POST, and edge cases.
Exits on first failure, prints ✅/❌ for each test.
"""
import os
import sys
import http.client
import socket
from urllib.parse import urlparse

# Server configuration (e.g. "http://localhost:8080")
SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")
# CGI endpoints to test
CGI_ENDPOINTS = [
    "/cgi-bin/hello.sh",
    "/cgi-bin/hello.py",
    "/cgi-bin/CgiEnv.py",
]
TIMEOUT = 5  # seconds for HTTPConnection


def request(method, path, body=None, headers=None):
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port, timeout=TIMEOUT)
    conn.request(method, path, body=body, headers=headers or {})
    res = conn.getresponse()
    data = res.read().decode(errors="replace")
    conn.close()
    return res.status, res.reason, data


def assert_cgi(method, path, expected_substring):
    body = "test-body" if method == "POST" else None
    headers = {"Content-Type": "text/plain"} if method == "POST" else {}
    status, reason, text = request(method, path, body=body, headers=headers)
    if status != 200:
        print(f"❌ {method} {path} → {status} {reason} (expected 200 OK)")
        sys.exit(1)
    if expected_substring not in text:
        print(f"❌ {method} {path} → missing '{expected_substring}' in response")
        sys.exit(1)
    if path.endswith("CgiEnv.py") and f"REQUEST_METHOD = {method}" not in text:
        print(f"❌ {method} {path} → REQUEST_METHOD not echoed correctly")
        sys.exit(1)
    print(f"✅ {method} {path} → 200 OK")


def assert_status(method, path, expected_code):
    try:
        status, reason, _ = request(method, path)
    except Exception as e:
        print(f"❌ {method} {path} → ERR: {e} (expected {expected_code})")
        sys.exit(1)
    if status != expected_code:
        print(f"❌ {method} {path} → {status} {reason} (expected {expected_code})")
        sys.exit(1)
    print(f"✅ {method} {path} → {status} {reason}")


def assert_timeout_or_504(method, path):
    try:
        status, reason, _ = request(method, path)
        if status == 504:
            print(f"✅ {method} {path} → 504 {reason}")
            return
        print(f"❌ {method} {path} → {status} {reason} (expected 504 or timeout)")
        sys.exit(1)
    except (socket.timeout, TimeoutError, http.client.RemoteDisconnected):
        print(f"✅ {method} {path} → timed out as expected")


def test_get():
    print("[ CGI GET Tests ]")
    for path in CGI_ENDPOINTS:
        expected = "REQUEST_METHOD =" if path.endswith("CgiEnv.py") else "Hello from"
        assert_cgi("GET", path, expected)


def test_post():
    print("[ CGI POST Tests ]")
    for path in CGI_ENDPOINTS:
        expected = "REQUEST_METHOD =" if path.endswith("CgiEnv.py") else "Hello from"
        assert_cgi("POST", path, expected)


def test_edge_cases():
    print("[ CGI Edge Case Tests ]")
    # 404 Not Found for missing script
    assert_status("GET", "/cgi-bin/missing.py", 404)
    # 403 Forbidden for non-executable script (make sure test_webserv/tester/data/cgi-bin/not_exec.py is chmod 644)
    assert_status("GET", "/cgi-bin/not_exec.py", 403)
    # 405 Method Not Allowed for unsupported method
    assert_status("DELETE", "/cgi-bin/hello.py", 405)
    # Query string should be ignored for script path
    assert_cgi("GET", "/cgi-bin/hello.py?foo=bar", "Hello from")
    # Wrong extension: not configured as CGI
    assert_status("GET", "/cgi-bin/hello.txt", 404)
    # Missing header delimiter → 500
    assert_status("GET", "/cgi-bin/bad_header.sh", 500)
    # CGI timeout → 504 or socket timeout
    assert_timeout_or_504("GET", "/cgi-bin/sleep.sh")


def main():
    test_get()
    test_post()
    test_edge_cases()


if __name__ == "__main__":
    main()
