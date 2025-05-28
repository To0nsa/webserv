import http.client
import os
import sys
from urllib.parse import urlparse

SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")


def request(path, headers=None):
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", path, headers=headers or {})
    response = conn.getresponse()
    body = response.read().decode(errors="replace")
    conn.close()
    return response.status, response.reason, body


def assert_status(path, expected_code):
    code, reason, _ = request(path)
    if code == expected_code:
        print(f"✅ GET {path} → {code} {reason}")
    else:
        print(f"❌ GET {path} → {code} {reason} (expected {expected_code})")
        sys.exit(1)


def assert_redirect(path, expected_location):
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", path)
    response = conn.getresponse()
    location = response.getheader("Location")
    if response.status == 301 and location == expected_location:
        print(f"✅ GET {path} → 301 Location: {location}")
    else:
        print(f"❌ GET {path} → {response.status}, Location: {location} (expected {expected_location})")
        sys.exit(1)
    conn.close()


def assert_content_type(path, expected_type):
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", path)
    res = conn.getresponse()
    content_type = res.getheader("Content-Type")
    if res.status == 200 and content_type == expected_type:
        print(f"✅ {path} → Content-Type: {content_type}")
    else:
        print(f"❌ {path} → Content-Type: {content_type} (expected {expected_type})")
        sys.exit(1)
    conn.close()


def test_invalid_http_version():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn._http_vsn = 2
    conn._http_vsn_str = "HTTP/2.0"
    conn.request("GET", "/index.html")
    res = conn.getresponse()
    if res.status == 505:
        print("✅ GET with HTTP/2.0 → 505 HTTP Version Not Supported")
    else:
        print(f"❌ GET with HTTP/2.0 → {res.status} (expected 505)")
        sys.exit(1)
    conn.close()


def test_header_case_insensitive():
    for variation in ["Host", "HOST", "host", "hOSt"]:
        code, _, _ = request("/index.html", headers={variation: "localhost:8080"})
        if code == 200:
            print(f"✅ Header case {variation} → 200 OK")
        else:
            print(f"❌ Header case {variation} → {code} (expected 200)")
            sys.exit(1)


def test_duplicate_headers():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.putrequest("GET", "/index.html", skip_host=True)
    conn.putheader("Host", "localhost:8080")
    conn.putheader("Host", "evil.com")
    conn.endheaders()
    res = conn.getresponse()
    if res.status == 400:
        print("✅ Duplicate Host headers → 400 Bad Request")
    else:
        print(f"❌ Duplicate Host headers → {res.status} (expected 400)")
        sys.exit(1)
    conn.close()


def test_long_url():
    long_path = "/a" * 2048
    code, _, _ = request(long_path)
    if code == 414:
        print(f"✅ Long URI → {code} URI Too Long")
    else:
        print(f"❌ Long URI → {code} (expected 400 or 414)")
        sys.exit(1)

def test_missing_host_header():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.putrequest("GET", "/index.html", skip_host=True)
    conn.endheaders()
    res = conn.getresponse()
    if res.status == 400:
        print("✅ Missing Host header → 400 Bad Request")
    else:
        print(f"❌ Missing Host header → {res.status} (expected 400)")
        sys.exit(1)
    conn.close()
    
def test_header_overflow():
    long_header = "a" * (8192 + 1) # HEADER_MAX_LENGTH + 1
    code, _, _ = request("/index.html", headers={"X-Test": long_header})
    assert code in (431, 400)
    print(f"✅ Oversized header → {code}")
    
def test_connection_close():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", "/index.html", headers={"Connection": "close"})
    res = conn.getresponse()
    assert res.getheader("Connection") == "close"
    print("✅ Connection: close honored")
    conn.close()
    
def test_if_modified_since():
    # first fetch to get Last-Modified
    _, _, _ = request("/index.html")
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    lm = request("/index.html")[2]  # or parse from headers
    conn.request("GET", "/index.html", headers={"If-Modified-Since": lm})
    res = conn.getresponse()
    assert res.status in (304, 200), "Expected 304 or 200"
    print(f"✅ Conditional GET → {res.status}")
    conn.close()
    
def test_get_with_body():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", "/index.html", body="useless body")
    res = conn.getresponse()
    assert res.status == 200
    print("✅ GET /index.html with body → 200 OK")
    conn.close()

def run_tests():
    print("[GET] Running GET tests...")

    # Static file serving
    assert_status("/index.html", 200)
    assert_status("/dir/file.txt", 200)
    assert_status("/", 200)
    assert_status("/dir//file.txt", 200)

    # Not found
    assert_status("/nonexistent", 404)
    assert_status("/dir/missing.html", 404)

    # Autoindex
    assert_status("/dir/", 200)
    assert_redirect("/dir", "/dir/")

    # Redirects
    assert_redirect("/forbidden", "/forbidden/")

    # Forbidden
    assert_status("/forbidden/", 403)
    
	# Query
    assert_status("/?foo=bar", 200)        # query shouldn’t change which file is served
    assert_status("/index.html?x=1&y=2", 200)
    assert_status("/index.html?foo=bar&x=1+1%3D2", 200)
    assert_status("/index.html?foo=bar#section1", 200)

    # Edge cases
    assert_status("//", 200)
    assert_status("//index.html", 200)
    assert_status("/../index.html", 403)
    assert_status("/%2E%2E/index.html", 403)   # %2E == .
    assert_status("/%2e%2e/%2e%2e/index.html", 403)

    # Content-Type validation
    assert_content_type("/index.html", "text/html")
    assert_content_type("/dir/file.txt", "text/plain")
    assert_content_type("/script.js", "application/javascript")
    assert_content_type("/style.css", "text/css")

    # Optional: binary file test
    code, _, body = request("/logo.png")
    if code == 200 and len(body) > 0:
        print(f"✅ GET /logo.png → 200 OK (binary, {len(body)} bytes)")
    else:
        print(f"❌ GET /logo.png → {code} (expected 200 with non-empty body)")
        sys.exit(1)


if __name__ == "__main__":
    run_tests()
    test_invalid_http_version()
    test_header_case_insensitive()
    test_duplicate_headers()
    test_long_url()
    test_missing_host_header()
    test_header_overflow()
    test_connection_close()
    test_if_modified_since()
    test_get_with_body()
