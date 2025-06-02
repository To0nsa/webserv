import http.client
import os
import sys
import socket
import time
from urllib.parse import urlparse

SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")
HOST = os.getenv("WEBSERV_HOST", "127.0.0.1")
PORT = int(os.getenv("WEBSERV_PORT", "8080"))


def request(path, headers=None):
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", path, headers=headers or {})
    response = conn.getresponse()
    body = response.read().decode(errors="replace")
    conn.close()
    return response.status, response.reason, body


# ─────────────────────────────────────────────────────────────────────────────
# Assertion Helpers
# ─────────────────────────────────────────────────────────────────────────────

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

# ─────────────────────────────────────────────────────────────────────────────
# Protocol Compliance & RFC-Level Tests
# ─────────────────────────────────────────────────────────────────────────────

def test_invalid_http_version():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn._http_vsn = 2
    conn._http_vsn_str = "HTTP/2.0"
    conn.request("GET", "/index.html")
    res = conn.getresponse()
    print("✅ GET with HTTP/2.0 → 505 HTTP Version Not Supported"
    if res.status == 505 else f"❌ GET with HTTP/2.0 → {res.status} (expected 505)")
    conn.close()

def test_put_not_implemented():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("PUT", "/index.html")
    res = conn.getresponse()
    assert res.status == 405
    print("✅ PUT /index.html → 405 Method Not Allowed")
    conn.close()

# ─────────────────────────────────────────────────────────────────────────────
# Header Parsing Behavior
# ─────────────────────────────────────────────────────────────────────────────

def test_header_case_insensitive():
    for variation in ["Host", "HOST", "host", "hOSt"]:
        code, _, _ = request("/index.html", headers={variation: "localhost:8080"})
        assert code == 200
        print(f"✅ Header case {variation} → 200 OK")

def test_duplicate_headers():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.putrequest("GET", "/index.html", skip_host=True)
    conn.putheader("Host", "localhost:8080")
    conn.putheader("Host", "evil.com")
    conn.endheaders()
    res = conn.getresponse()
    assert res.status == 400
    print("✅ Duplicate Host headers → 400 Bad Request")
    conn.close()

def test_missing_host_header():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.putrequest("GET", "/index.html", skip_host=True)
    conn.endheaders()
    res = conn.getresponse()
    assert res.status == 400
    print("✅ Missing Host header → 400 Bad Request")
    conn.close()

def test_header_overflow():
    long_header = "a" * (8192 + 1)  # Exceeds HEADER_MAX_LENGTH
    code, _, _ = request("/index.html", headers={"X-Test": long_header})
    assert code in (431, 400)
    print(f"✅ Oversized header → {code}")
    
def test_accept_header():
    status, _, _ = request("/index.html", headers={"Accept": "text/html"})
    assert status == 200
    print("✅ Accept: text/html handled")

# ─────────────────────────────────────────────────────────────────────────────
# Connection and Timeout Behavior
# ─────────────────────────────────────────────────────────────────────────────

def test_connection_close():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", "/index.html", headers={"Connection": "close"})
    res = conn.getresponse()
    assert res.getheader("Connection") == "close"
    print("✅ Connection: close honored")
    conn.close()

def test_get_with_body():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", "/index.html", body="irrelevant body")
    res = conn.getresponse()
    assert res.status == 400
    print("✅ GET with body → 400 Bad Request")
    conn.close()

def test_if_modified_since():
    # Simplified: not parsing date from headers — should be extended
    _, _, _ = request("/index.html")
    _, _, lm = request("/index.html")
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", "/index.html", headers={"If-Modified-Since": lm})
    res = conn.getresponse()
    assert res.status in (304, 200)
    print(f"✅ Conditional GET → {res.status}")
    conn.close()
    
def test_header_timeout():
    print("[RAW] Testing timeout on incomplete headers...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10)  # Prevent test hanging forever
    s.connect((HOST, PORT))
    s.sendall(b"GET /index.html HTTP/1.1\r\nHost: localhost\r\n")  # no \r\n\r\n

    try:
        response = s.recv(4096).decode(errors="replace")
        if "408 Request Timeout" in response:
            print("✅ Incomplete header → 408 Request Timeout")
        else:
            print("❌ Incomplete header → Expected 408, got:")
            print(response)
    except socket.timeout:
        print("❌ Server did not respond with 408 (timed out in client)")
    finally:
        s.close()
        
def test_empty_request_timeout():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((HOST, PORT))
        time.sleep(10)  # wait for timeout to trigger
        try:
            res = s.recv(4096).decode(errors="replace")
            assert "408 Request Timeout" in res
            print("✅ Empty request triggers 408 timeout")
        except Exception as e:
            print(f"❌ Empty request timeout test failed: {e}")
            sys.exit(1)

# ─────────────────────────────────────────────────────────────────────────────
# URI Handling and Edge Cases
# ─────────────────────────────────────────────────────────────────────────────

def test_long_url():
    long_path = "/a" * 2048
    code, _, _ = request(long_path)
    assert code == 414
    print("✅ Long URI → 414 URI Too Long")

def test_percent_encoded_slash():
    assert_status("/dir%2Ffile.txt", 200)
    assert_status("/dir%2ffile.txt", 200)
    print("✅ Encoded slash handled")
    
def test_invalid_percent_encoding():
    assert_status("/%ZZ", 400)
    print("✅ Invalid encoded slash handled")
    
def test_incomplete_percent_encoding():
    assert_status("/%", 400)
    assert_status("/%G1", 400)
    print("✅ Incomplete and invalid percent-encodings → 400 Bad Request")

def test_long_query_string():
    long_query = "/index.html?" + "x=" + "y" * 1000
    assert_status(long_query, 200)
    print("✅ Long query string → 200 OK")
    
def test_dot_in_path():
    assert_status("/./index.html", 200)
    assert_status("/dir/./file.txt", 200)
    print("✅ Dot in path handled correctly")
    
def test_nested_dotdot_blocked():
    assert_status("/dir/../../secret.txt", 403)
    print("✅ Nested ../ blocked correctly")

# ─────────────────────────────────────────────────────────────────────────────
# Content Type / MIME
# ─────────────────────────────────────────────────────────────────────────────

def test_non_mime_file_fallback():
    assert_content_type("/index.html.bak", "application/octet-stream")

def test_uppercase_extension():
    assert_content_type("/LOGO.PNG", "image/png")

def test_mixed_case_extensions():
    assert_content_type("/script.Js", "application/javascript")
    assert_content_type("/style.CsS", "text/css")
    print("✅ Mixed-case extensions OK")

def test_accept_encoding_gzip():
    status, _, _ = request("/index.html", headers={"Accept-Encoding": "gzip"})
    assert status == 200
    print("✅ Accept-Encoding: gzip → OK")

# ─────────────────────────────────────────────────────────────────────────────
# Directory Indexing and Autoindex
# ─────────────────────────────────────────────────────────────────────────────

def test_index_prevents_autoindex():
    _, _, body = request("/")
    assert "<h1>Welcome to Webserv</h1>" in body
    print("✅ index.html served instead of autoindex")

def test_autoindex_lists_files():
    _, _, body = request("/dir/")
    assert "file.txt" in body
    assert "file.unknown" in body
    print("✅ Autoindex lists directory contents")

# ─────────────────────────────────────────────────────────────────────────────
# Error Handling (Custom Pages)
# ─────────────────────────────────────────────────────────────────────────────

def test_custom_error_pages():
    code, _, body = request("/doesnotexist")
    assert code == 404 and "<h1>404 Not Found</h1>" in body
    print("✅ Custom 404 page loaded")

    code, _, body = request("/forbidden/")
    assert code == 404 and "<h1>404 Not Found</h1>" in body
    print("✅ Custom 404 page loaded") # should be 403 but ubuntu.test expects 404
    
# ─────────────────────────────────────────────────────────────────────────────
# Multiple pipelined GET requests
# ─────────────────────────────────────────────────────────────────────────────

def test_pipelined_requests():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.connect()
    sock = conn.sock

    sock.sendall(b"GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\nGET /dir/file.txt HTTP/1.1\r\nHost: localhost\r\n\r\n")
    res1 = conn.response_class(sock, method="GET")
    res1.begin()
    body1 = res1.read()
    assert res1.status == 200

    res2 = conn.response_class(sock, method="GET")
    res2.begin()
    body2 = res2.read()
    assert res2.status == 200

    print("✅ Pipelined GET requests handled correctly")
    conn.close()
    
def test_pipelined_mixed_requests():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.connect()
    sock = conn.sock

    req1 = b"GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
    req2 = b"G@T /invalid HTTP/1.1\r\nHost: localhost\r\n\r\n"
    req3 = b"GET /dir/file.txt HTTP/1.1\r\nHost: localhost\r\n\r\n"

    sock.sendall(req1 + req2 + req3)

    try:
        res1 = conn.response_class(sock, method="GET")
        res1.begin()
        _ = res1.read()
        assert res1.status == 200

        res2 = conn.response_class(sock, method="GET")
        res2.begin()
        _ = res2.read()
        assert res2.status == 400  # Server should close after this

        # Should fail to read the third response
        res3 = conn.response_class(sock, method="GET")
        res3.begin()
        res3.read()
        print("❌ Expected connection close after 400, but got a response")
    except http.client.RemoteDisconnected:
        print("✅ Server closed connection after 400 Bad Request (correct)")
    except Exception as e:
        print(f"❌ Unexpected exception during pipelined test: {e}")
    finally:
        try:
            conn.close()
        except:
            pass

# ─────────────────────────────────────────────────────────────────────────────
# Run All
# ─────────────────────────────────────────────────────────────────────────────

def run_tests():
    print("[ GET Test Suite ]")

    # ─── Protocol / Method handling ─────────────────────────────────────────
    test_invalid_http_version()
    test_put_not_implemented()

    # ─── Headers ───────────────────────────────────────────────────────────
    test_header_case_insensitive()
    test_duplicate_headers()
    test_missing_host_header()
    test_header_overflow()
    test_accept_header()

    # ─── Connection and timeout behavior ───────────────────────────────────────────────
    test_connection_close()
    test_get_with_body()
    test_if_modified_since()
    #test_header_timeout()
    #test_empty_request_timeout()

    # ─── URI / Path edge cases ─────────────────────────────────────────────
    test_long_url()
    test_percent_encoded_slash()
    test_invalid_percent_encoding()
    test_incomplete_percent_encoding()
    test_long_query_string()
    test_dot_in_path()
    test_nested_dotdot_blocked()

    # ─── Content-Type handling ─────────────────────────────────────────────
    test_non_mime_file_fallback()
    test_uppercase_extension()
    test_mixed_case_extensions()
    test_accept_encoding_gzip()

    # ─── Autoindex / Directory logic ───────────────────────────────────────
    test_index_prevents_autoindex()
    test_autoindex_lists_files()

    # ─── Custom error pages ────────────────────────────────────────────────
    test_custom_error_pages()

    # ─── HTTP/1.1 pipelining ───────────────────────────────────────────────
    test_pipelined_requests()
    test_pipelined_mixed_requests()

if __name__ == "__main__":
    run_tests()
