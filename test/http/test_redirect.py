import http.client
import os
import sys
from urllib.parse import urlparse

SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")

# ─────────────────────────────────────────────────────────────────────────────
# Assertion Helpers
# ─────────────────────────────────────────────────────────────────────────────

def assert_status(path, expected_code, method="GET", body=None, headers=None):
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request(method, path, body=body or "", headers=headers or {})
    res = conn.getresponse()
    if res.status == expected_code:
        print(f"✅ {method} {path} → {expected_code}")
    else:
        print(f"❌ {method} {path} → {res.status} {res.reason} (expected {expected_code})")
        sys.exit(1)
    conn.close()

def assert_redirect(path, expected_location, method="GET"):
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request(method, path)
    res = conn.getresponse()
    location = res.getheader("Location")
    if res.status == 301 and location == expected_location:
        print(f"✅ {method} {path} → 301 Location: {location}")
    else:
        print(f"❌ {method} {path} → {res.status} {res.reason}, Location: {location!r} (expected {expected_location!r})")
        sys.exit(1)
    conn.close()

# ─────────────────────────────────────────────────────────────────────────────
# Redirection Tests
# ─────────────────────────────────────────────────────────────────────────────

def test_trailing_slash_redirect():
    """
    GET /dir (no trailing slash) → expect 301 and Location: /dir/
    Then GET /dir/ → expect 200 OK (no redirect)
    """
    assert_redirect("/dir", "/dir/")
    assert_status("/dir/", 200)

def test_non_get_methods_do_not_redirect():
    """
    POST /dir → should return 405 Method Not Allowed (no redirect)
    DELETE /dir → should return 403 Forbidden (no redirect to /dir/)
    """
    parsed = urlparse(SERVER)

    # POST /dir with a small body → 405
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    body = "test"
    headers = {
        "Content-Type": "text/plain",
        "Content-Length": str(len(body))
    }
    conn.request("POST", "/dir", body=body, headers=headers)
    res = conn.getresponse()
    if res.status == 405:
        print("✅ POST /dir → 405 Method Not Allowed (no redirect)")
    else:
        print(f"❌ POST /dir → {res.status} {res.reason} (expected 405)")
        sys.exit(1)
    conn.close()

    # DELETE /dir → 403
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("DELETE", "/dir")
    res = conn.getresponse()
    if res.status == 403:
        print("✅ DELETE /dir → 403 Forbidden (no redirect)")
    else:
        print(f"❌ DELETE /dir → {res.status} {res.reason} (expected 403)")
        sys.exit(1)
    conn.close()

def test_no_redirect_on_other_paths():
    """
    GET / (root) should not redirect; expect 200 OK
    GET /doesnotexist → expect 404 (no redirect)
    """
    assert_status("/", 200)

    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", "/doesnotexist")
    res = conn.getresponse()
    if res.status == 404:
        print("✅ GET /doesnotexist → 404 Not Found (no redirect)")
    else:
        print(f"❌ GET /doesnotexist → {res.status} {res.reason} (expected 404)")
        sys.exit(1)
    conn.close()

# ─────────────────────────────────────────────────────────────────────────────
# Edge Case Tests
# ─────────────────────────────────────────────────────────────────────────────

def test_query_string_preserved_or_dropped():
    """
    GET /dir?foo=bar → expect 301 and Location: /dir/
    (query string either preserved or dropped depending on implementation)
    For strict match, we only assert the 301 and base path.
    """
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", "/dir?foo=bar")
    res = conn.getresponse()
    location = res.getheader("Location")
    if res.status == 301 and location.startswith("/dir/"):
        print(f"✅ GET /dir?foo=bar → 301 Location: {location}")
    else:
        print(f"❌ GET /dir?foo=bar → {res.status} {res.reason}, Location: {location!r} (expected 301, '/dir/...')")
        sys.exit(1)
    conn.close()

def test_nonexact_prefix_no_redirect():
    """
    GET /dirextra → expect 404 (no redirect)
    """
    assert_status("/dirextra", 404)

def test_case_sensitivity_and_invalid_paths():
    """
    GET /DIR → expect 404 (case-sensitive)
    GET /dir/../ → expect 404 or normalized handling (no redirect)
    """
    assert_status("/DIR", 404)
    assert_status("/dir/../", 404)

def test_head_method_on_redirect():
    """
    HEAD /dir → expect 301 and Location: /dir/
    HEAD /dir/ → expect 200
    """
    assert_redirect("/dir", "/dir/", method="HEAD")
    assert_status("/dir/", 200, method="HEAD")

def test_post_to_directory_path_with_slash():
    """
    POST /dir/ → expect 405 (method not allowed, no redirect)
    """
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    body = "data"
    headers = {
        "Content-Type": "text/plain",
        "Content-Length": str(len(body))
    }
    conn.request("POST", "/dir/", body=body, headers=headers)
    res = conn.getresponse()
    if res.status == 405:
        print("✅ POST /dir/ → 405 Method Not Allowed (no redirect)")
    else:
        print(f"❌ POST /dir/ → {res.status} {res.reason} (expected 405)")
        sys.exit(1)
    conn.close()

def test_delete_to_redirect_source():
    """
    DELETE /dir → expect 403 (no redirect)
    DELETE /dir/ → expect 200 if resource exists or 204 if empty; at minimum, not a redirect
    """
    # DELETE /dir → 403
    assert_status("/dir", 403, method="DELETE")

    # DELETE /dir/ → resource handling (we assert not 301)
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("DELETE", "/dir/")
    res = conn.getresponse()
    if res.status in (200, 204, 403, 404):
        print(f"✅ DELETE /dir/ → {res.status} (no redirect)")
    else:
        print(f"❌ DELETE /dir/ → {res.status} {res.reason} (unexpected)")
        sys.exit(1)
    conn.close()

if __name__ == "__main__":
    print("\n[REDIRECTION TESTS] Starting...\n")
    test_trailing_slash_redirect()
    test_non_get_methods_do_not_redirect()
    test_no_redirect_on_other_paths()

    # Edge cases
    test_query_string_preserved_or_dropped()
    test_nonexact_prefix_no_redirect()
    test_case_sensitivity_and_invalid_paths()
    test_head_method_on_redirect()
    test_post_to_directory_path_with_slash()
    test_delete_to_redirect_source()
