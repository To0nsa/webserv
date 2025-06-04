import http.client
import os
import sys
import time
import socket
from urllib.parse import urlparse

SERVER     = os.getenv("WEBSERV_URL", "http://localhost:8080")
UPLOAD_DIR = os.getenv("UPLOAD_DIR", "./test/data/upload_store")

def log_request(method, path, status, reason, expected=None):
    if expected is not None and status != expected:
        print(f"❌ {method} {path} → {status} {reason} (expected {expected})")
        sys.exit(1)
    tag = "✅" if method == "POST" and status in (201, 400, 403, 405, 413, 414, 415) else "[LOG]"
    print(f"{tag} {method} {path} → {status} {reason}")

def request(method, path, body=None, headers=None, expected=None):
    """Send an HTTP request to SERVER and assert status if expected is provided."""
    parsed = urlparse(SERVER)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request(method, path, body=body, headers=headers or {})
    res    = conn.getresponse()
    data   = res.read().decode(errors="replace")
    conn.close()
    log_request(method, path, res.status, res.reason, expected)
    return res.status, res.reason, data

def send_raw(request_str):
    """
    Send a raw HTTP request over a socket and return (status_code, reason, raw_response).
    Reads only up to the indicated Content-Length then returns.
    """
    parsed = urlparse(SERVER)
    host = parsed.hostname
    port = parsed.port
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    sock.sendall(request_str.encode())
    # Read status line + headers
    response = b""
    while b"\r\n\r\n" not in response:
        chunk = sock.recv(4096)
        if not chunk:
            break
        response += chunk
    header_part, rest = response.split(b"\r\n\r\n", 1)
    # Parse Content-Length if present
    headers = header_part.decode(errors="replace").split("\r\n")[1:]
    content_length = 0
    for h in headers:
        if h.lower().startswith("content-length:"):
            try:
                content_length = int(h.split(":", 1)[1].strip())
            except:
                content_length = 0
            break
    # Read body based on Content-Length
    body = rest
    to_read = content_length - len(rest)
    while to_read > 0:
        chunk = sock.recv(4096)
        if not chunk:
            break
        body += chunk
        to_read -= len(chunk)
    sock.close()
    status_line = header_part.split(b"\r\n", 1)[0].decode(errors="replace")
    parts = status_line.split(" ", 2)
    try:
        status_code = int(parts[1])
        reason = parts[2] if len(parts) > 2 else ""
    except:
        status_code = 0
        reason = ""
    return status_code, reason, (header_part + b"\r\n\r\n" + body).decode(errors="replace")

def ensure_absent(local_path):
    """
    Ensure that any existing file on disk (under UPLOAD_DIR) is removed
    before running a particular upload test.
    """
    candidate = os.path.join(UPLOAD_DIR, os.path.basename(local_path))
    if os.path.isfile(candidate):
        try:
            os.remove(candidate)
        except OSError:
            # If server has already created it, delete via HTTP DELETE
            request("DELETE", local_path, expected=200)

def test_raw_body_upload():
    """
    1) POST raw body with Content-Type: text/plain to /upload_store/raw.txt → 201 Created
    2) GET /upload_store/raw.txt → verify content matches
    3) DELETE /upload_store/raw.txt → cleanup
    """
    path = "/upload_store/raw.txt"
    ensure_absent(path)

    status, reason, _ = request(
        "POST",
        path,
        body="Hello, Webserv!",
        headers={"Content-Type": "text/plain"},
        expected=201
    )

    parsed = urlparse(SERVER)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", path)
    res  = conn.getresponse()
    body = res.read().decode(errors="replace")
    conn.close()
    if res.status != 200 or "Hello, Webserv!" not in body:
        print(f"❌ GET {path} → {res.status} (expected 200) or body mismatch")
        sys.exit(1)
    print(f"✅ GET {path} → 200 OK, body verified")

    request("DELETE", path, expected=200)

def test_urlencoded_form_upload():
    """
    1) POST a simple URL-encoded form to /upload_store/form.html → 201 Created
    2) GET /upload_store/form.html → verify that the server wrote an HTML file containing form data
    3) DELETE /upload_store/form.html → cleanup
    """
    path = "/upload_store/form.html"
    ensure_absent(path)

    form_body = "field1=value1&field2=value+two"
    status, reason, _ = request(
        "POST",
        path,
        body=form_body,
        headers={"Content-Type": "application/x-www-form-urlencoded; charset=UTF-8"},
        expected=201
    )

    parsed = urlparse(SERVER)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", path)
    res  = conn.getresponse()
    body = res.read().decode(errors="replace")
    conn.close()

    if res.status != 200 or "value1" not in body or "value two" not in body:
        print(f"❌ GET {path} → {res.status} (expected 200) or content mismatch")
        sys.exit(1)
    print(f"✅ GET {path} → 200 OK, form fields verified")

    request("DELETE", path, expected=200)

def test_multipart_form_upload():
    """
    1) Construct and POST a multipart/form-data (single file) to "/upload_store/"
    2) Verify server creates "/upload_store/upload.txt" with correct content
    3) DELETE "/upload_store/upload.txt" → cleanup
    """
    target_filename = "upload.txt"
    target_path     = f"/upload_store/{target_filename}"
    ensure_absent(target_path)

    boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW"
    multipart_body = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="file"; filename="{target_filename}"\r\n'
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Multipart upload content!\r\n"
        f"--{boundary}--\r\n"
    )

    status, reason, _ = request(
        "POST",
        "/upload_store/",
        body=multipart_body,
        headers={"Content-Type": f"multipart/form-data; boundary={boundary}"},
        expected=201
    )

    parsed = urlparse(SERVER)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", target_path)
    res  = conn.getresponse()
    body = res.read().decode(errors="replace")
    conn.close()

    if res.status != 200 or "Multipart upload content!" not in body:
        print(f"❌ GET {target_path} → {res.status} or content mismatch")
        sys.exit(1)
    print(f"✅ GET {target_path} → 200 OK, multipart content verified")

    request("DELETE", target_path, expected=200)

def test_missing_content_type():
    """
    POST without a Content-Type header to /upload_store/shouldfail.txt → 415 Unsupported Media Type
    Ensure no file is created
    """
    path = "/upload_store/shouldfail.txt"
    ensure_absent(path)

    status, reason, _ = request(
        "POST",
        path,
        body="no content type",
        headers={},  # intentionally missing Content-Type
        expected=415
    )

    local = os.path.join(UPLOAD_DIR, os.path.basename(path))
    if os.path.isfile(local):
        print(f"❌ File {local} should not exist after 415 response")
        sys.exit(1)
    print("✅ 415 returned and no file created")

def test_upload_to_non_upload_location():
    """
    POST to /dir/file.txt (a non-upload location) → 405 Method Not Allowed
    Ensure no file is created under test/data/dir/
    """
    path = "/dir/file.txt"
    local = os.path.join("test/data/dir", "file.txt")
    if os.path.isfile(local):
        os.remove(local)

    status, reason, _ = request(
        "POST",
        path,
        body="should not be allowed",
        headers={"Content-Type": "text/plain"},
        expected=405
    )

    if os.path.isfile(local):
        print(f"❌ File {local} should not exist under /dir/")
        sys.exit(1)
    print("✅ 405 returned for non-upload POST and no file created")

def test_path_traversal_upload():
    """
    POST to /upload_store/../outside.txt → 403 Forbidden (reject traversal)
    Ensure no file is created outside upload_store
    """
    path = "/upload_store/../outside.txt"
    local = os.path.normpath(os.path.join(UPLOAD_DIR, "../outside.txt"))
    with open(local, "w") as f:
        f.write("I should remain")

    status, reason, _ = request(
        "POST",
        path,
        body="hacker content",
        headers={"Content-Type": "text/plain"},
        expected=403
    )

    if not os.path.isfile(local):
        print(f"❌ {local} should still exist after traversal attempt")
        sys.exit(1)
    print("✅ 403 returned and outside file left intact")

    os.remove(local)

def test_overwrite_existing():
    """
    1) Create /upload_store/existing.txt via POST
    2) POST again to the same path → 400 Bad Request (server must refuse overwrite)
    3) DELETE /upload_store/existing.txt → cleanup
    """
    path = "/upload_store/existing.txt"
    ensure_absent(path)

    request(
        "POST",
        path,
        body="first content",
        headers={"Content-Type": "text/plain"},
        expected=201
    )

    status, reason, _ = request(
        "POST",
        path,
        body="second content",
        headers={"Content-Type": "text/plain"},
        expected=400
    )

    request("DELETE", path, expected=200)

def test_percent_encoded_filename():
    """
    POST to /upload_store/file%20with%20space.txt → 201 Created
    Then GET /upload_store/file with space.txt → verify content
    Finally DELETE → cleanup
    """
    uri  = "/upload_store/file%20with%20space.txt"
    fs_name = "file with space.txt"
    ensure_absent(f"/upload_store/{fs_name}")

    status, reason, _ = request(
        "POST",
        uri,
        body="percent encoded",
        headers={"Content-Type": "text/plain"},
        expected=201
    )

    parsed = urlparse(SERVER)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", uri)
    res  = conn.getresponse()
    body = res.read().decode(errors="replace")
    conn.close()

    if res.status != 200 or "percent encoded" not in body:
        print(f"❌ GET {uri} → {res.status} or content mismatch")
        sys.exit(1)
    print("✅ Percent-encoded upload verified")

    request("DELETE", uri, expected=200)

def test_nested_directory_upload():
    """
    POST to /upload_store/deep/nested/dir/file.txt → 201 Created
    Verify GET → 200, then DELETE → cleanup.
    """
    path = "/upload_store/deep/nested/dir/file.txt"
    nested_local = os.path.join(UPLOAD_DIR, "deep/nested/dir/file.txt")
    if os.path.isfile(nested_local):
        os.remove(nested_local)

    status, reason, _ = request(
        "POST",
        path,
        body="deep content",
        headers={"Content-Type": "text/plain"},
        expected=201
    )

    parsed = urlparse(SERVER)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", path)
    res  = conn.getresponse()
    body = res.read().decode(errors="replace")
    conn.close()

    if res.status != 200 or "deep content" not in body:
        print(f"❌ GET {path} → {res.status} or content mismatch")
        sys.exit(1)
    print("✅ Nested upload verified")

    request("DELETE", path, expected=200)

def test_long_url_too_long():
    """
    POST with a very long URI (length > 2048) → 414 Request-URI Too Long
    """
    long_path = "/" + "a" * 2050
    request("POST", long_path, body="x", headers={"Content-Type": "text/plain"}, expected=414)

def test_chunked_transfer_encoding():
    """
    1) POST with Transfer-Encoding: chunked → 201 Created
    2) GET → verify assembled body
    """
    target = "/upload_store/chunked.txt"
    ensure_absent(target)

    chunked_body = (
        "7\r\nMozilla\r\n"
        "9\r\nDeveloper\r\n"
        "7\r\nNetwork\r\n"
        "0\r\n\r\n"
    )
    request_str = (
        f"POST {target} HTTP/1.1\r\n"
        f"Host: {urlparse(SERVER).hostname}\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        f"{chunked_body}"
    )
    status, reason, _ = send_raw(request_str)
    if status != 201:
        print(f"❌ RAW POST chunked {target} → {status} (expected 201)")
        sys.exit(1)
    print(f"✅ RAW POST chunked {target} → 201 Created")

    parsed = urlparse(SERVER)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", target)
    res  = conn.getresponse()
    body = res.read().decode(errors="replace")
    conn.close()
    if res.status != 200 or "MozillaDeveloperNetwork" not in body:
        print(f"❌ GET {target} → {res.status} or body mismatch")
        sys.exit(1)
    print(f"✅ GET {target} → 200 OK, chunked body verified")

    request("DELETE", target, expected=200)

def test_malformed_chunked_encoding():
    """
    POST with malformed chunk size → 400 Bad Request
    """
    target = "/upload_store/badchunk.txt"
    ensure_absent(target)

    bad_chunk = (
        "Z\r\nBadData\r\n"
        "0\r\n\r\n"
    )
    request_str = (
        f"POST {target} HTTP/1.1\r\n"
        f"Host: {urlparse(SERVER).hostname}\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        f"{bad_chunk}"
    )
    status, reason, _ = send_raw(request_str)
    if status != 400:
        print(f"❌ RAW POST malformed chunk {target} → {status} (expected 400)")
        sys.exit(1)
    print("✅ RAW POST malformed chunk → 400 Bad Request")

def test_missing_final_chunk():
    """
    POST chunked without terminating 0-length chunk → 400 Bad Request
    """
    target = "/upload_store/nozero.txt"
    ensure_absent(target)

    partial_chunks = (
        "5\r\nHello\r\n"
        # missing "0\r\n\r\n"
    )
    request_str = (
        f"POST {target} HTTP/1.1\r\n"
        f"Host: {urlparse(SERVER).hostname}\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        f"{partial_chunks}"
    )
    status, reason, _ = send_raw(request_str)
    if status != 408:
        print(f"❌ RAW POST missing final chunk {target} → {status} (expected 408)")
        sys.exit(1)
    print("✅ RAW POST missing final chunk → 408 Timeout")

def test_expect_100_continue():
    """
    POST with Expect: 100-continue → server replies 417 Expectation Failed
    and does not read the body.
    """
    target = "/upload_store/expect.txt"
    ensure_absent(target)

    parsed = urlparse(SERVER)
    host = parsed.hostname
    port = parsed.port
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))

    # Send headers with Expect: 100-continue (no body)
    header_block = (
        f"POST {target} HTTP/1.1\r\n"
        f"Host: {host}\r\n"
        "Connection: close\r\n"
        "Content-Length: 11\r\n"
        "Expect: 100-continue\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
    )
    sock.sendall(header_block.encode())

    # Read the single response (should be 417 Expectation Failed)
    response = b""
    while True:
        chunk = sock.recv(4096)
        if not chunk:
            break
        response += chunk
    sock.close()

    # Parse status line
    status_line = response.split(b"\r\n", 1)[0].decode(errors="replace")
    parts = status_line.split(" ", 2)
    try:
        status = int(parts[1])
    except:
        status = 0

    if status != 417:
        print(f"❌ Expected 417 Expectation Failed, got: {status_line}")
        sys.exit(1)
    print("✅ Received 417 Expectation Failed")

    # Verify that no file was created
    parsed = urlparse(SERVER)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", target)
    res  = conn.getresponse()
    conn.close()
    if res.status != 404:
        print(f"❌ GET {target} → {res.status} (expected 404, no file created)")
        sys.exit(1)
    print("✅ No file created after Expectation Failed")

def test_content_length_and_chunked_conflict():
    """
    POST with both Content-Length and Transfer-Encoding: chunked → 400 Bad Request (server rejects conflict).
    """
    target = "/upload_store/conflict.txt"
    ensure_absent(target)

    chunked_body = (
        "5\r\nHello\r\n"
        "0\r\n\r\n"
    )
    request_str = (
        f"POST {target} HTTP/1.1\r\n"
        f"Host: {urlparse(SERVER).hostname}\r\n"
        "Connection: close\r\n"
        "Content-Length: 5\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        f"{chunked_body}"
    )
    status, reason, _ = send_raw(request_str)
    if status != 400:
        print(f"❌ RAW POST conflict headers {target} → {status} (expected 400)")
        sys.exit(1)
    print("✅ RAW POST both Content-Length and chunked → 400 Bad Request")

    # Verify that no file was created: accept either 404 Not Found or 301 Moved Permanently
    parsed = urlparse(SERVER)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", target)
    res  = conn.getresponse()
    conn.close()
    if res.status not in (404, 301):
        print(f"❌ GET {target} → {res.status} (expected 404 or 301, no file created)")
        sys.exit(1)
    print(f"✅ GET conflict test → {res.status} (no file created)")

def test_conflicting_content_length_headers():
    """
    POST with two different Content-Length headers → 400 Bad Request
    """
    target = "/upload_store/dupcl.txt"
    ensure_absent(target)

    body = "DUPLICATE"
    request_str = (
        f"POST {target} HTTP/1.1\r\n"
        f"Host: {urlparse(SERVER).hostname}\r\n"
        "Connection: close\r\n"
        "Content-Length: 9\r\n"
        "Content-Length: 5\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        f"{body}"
    )
    status, reason, _ = send_raw(request_str)
    if status != 400:
        print(f"❌ RAW POST duplicate CL {target} → {status} (expected 400)")
        sys.exit(1)
    print("✅ RAW POST duplicate Content-Length → 400 Bad Request")

def test_empty_body_no_content_length():
    """
    POST without Content-Length and no Transfer-Encoding → 411 Length Required
    and no file is created.
    """
    target = "/upload_store/empty.txt"
    ensure_absent(target)

    request_str = (
        f"POST {target} HTTP/1.1\r\n"
        f"Host: {urlparse(SERVER).hostname}\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        # no body
    )
    status, reason, _ = send_raw(request_str)
    if status != 411:
        print(f"❌ RAW POST empty body {target} → {status} (expected 411)")
        sys.exit(1)
    print("✅ RAW POST empty body → 411 Length Required")

    # Verify that no file was created
    parsed = urlparse(SERVER)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", target)
    res  = conn.getresponse()
    conn.close()
    if res.status not in (404, 301):
        print(f"❌ GET {target} → {res.status} (expected 404 or 301, no file created)")
        sys.exit(1)
    print(f"✅ GET empty test → {res.status} (no file created)")


def test_multipart_missing_boundary():
    """
    POST multipart/form-data without boundary → 400 Bad Request
    """
    target = "/upload_store/noboundary.txt"
    ensure_absent(target)

    body = "--notused\r\nContent-Disposition: form-data; name=\"file\"; filename=\"a.txt\"\r\n\r\nHi\r\n--notused--\r\n"
    status, reason, _ = request(
        "POST",
        target,
        body=body,
        headers={"Content-Type": "multipart/form-data"},  # missing boundary
        expected=400
    )
    print("✅ Multipart missing boundary → 400 Bad Request")

def test_unsupported_media_type_xml():
    """
    POST with Content-Type: application/xml → 415 Unsupported Media Type
    """
    target = "/upload_store/data.xml"
    ensure_absent(target)

    xml_body = "<root>Test</root>"
    status, reason, _ = request(
        "POST",
        target,
        body=xml_body,
        headers={"Content-Type": "application/xml"},
        expected=415
    )
    print("✅ Unsupported media type (XML) → 415")

def test_line_ending_lf_only():
    """
    POST headers terminated by LF only → 400 Bad Request or 408 Request Timeout
    """
    target = "/upload_store/lf.txt"
    ensure_absent(target)

    raw = (
        f"POST {target} HTTP/1.1\n"
        f"Host: {urlparse(SERVER).hostname}\n"
        "Content-Length: 5\n"
        "Content-Type: text/plain\n"
        "\n"
        "Hello"
    )
    status, reason, _ = send_raw(raw)
    if status not in (400, 408):
        print(f"❌ RAW POST LF-only {target} → {status} (expected 400 or 408)")
        sys.exit(1)
    print(f"✅ RAW POST LF-only → {status} {'Bad Request' if status == 400 else 'Request Timeout'}")


def test_header_folding_rejection():
    """
    POST with a folded header (obsolete format) → 400 Bad Request
    """
    target = "/upload_store/folded.txt"
    ensure_absent(target)

    raw = (
        f"POST {target} HTTP/1.1\r\n"
        f"Host: {urlparse(SERVER).hostname}\r\n"
        "Content-Type:\r\n"
        " text/plain\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "Hello"
    )
    status, reason, _ = send_raw(raw)
    if status != 400:
        print(f"❌ RAW POST folded header {target} → {status} (expected 400)")
        sys.exit(1)
    print("✅ RAW POST folded header → 400 Bad Request")

def test_max_body_size_content_length():
    """
    POST with Content-Length just over client_max_body_size → 413 Payload Too Large (or 431).
    The server may close the connection mid‐send, causing BrokenPipeError or ConnectionResetError.
    We treat that as acceptable.
    """
    target = "/upload_store/large_cl.txt"
    # Assume client_max_body_size = 1MB
    size = 1024 * 1024 + 1
    body = "A" * size

    try:
        status, reason, _ = request(
            "POST",
            target,
            body=body,
            headers={"Content-Type": "text/plain", "Content-Length": str(size)},
            expected=None
        )
    except (BrokenPipeError, ConnectionResetError):
        # Server closed the connection when seeing the oversized Content-Length.
        print("✅ Server closed connection on oversized Content-Length → acceptable behavior")
        return

    if status not in (413, 431):
        print(f"❌ RAW POST large Content-Length {target} → {status} (expected 413 or 431)")
        sys.exit(1)
    print(f"✅ RAW POST large Content-Length → {status} {'Payload Too Large' if status == 413 else 'Request Header Fields Too Large'}")

def test_max_body_size_chunked():
    """
    POST chunked body that exceeds 1MB → 413 Payload Too Large mid-transfer
    """
    target = "/upload_store/large_chunked.txt"
    ensure_absent(target)

    # Create chunks of 1024KB twice, totaling 1MB+ → first chunk okay, second should trigger 413
    hex_512k = hex(1024 * 1024)[2:]
    chunk1 = f"{hex_512k}\r\n" + ("B" * (1024 * 1024)) + "\r\n"
    chunk2 = f"{hex_512k}\r\n" + ("C" * (1024 * 1024)) + "\r\n"
    term = "0\r\n\r\n"
    raw = (
        f"POST {target} HTTP/1.1\r\n"
        f"Host: {urlparse(SERVER).hostname}\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        f"{chunk1}{chunk2}{term}"
    )
    status, reason, _ = send_raw(raw)
    if status != 413:
        print(f"❌ RAW POST large chunked {target} → {status} (expected 413)")
        sys.exit(1)
    print("✅ RAW POST large chunked → 413 Payload Too Large")

def test_content_length_mismatch():
    """
    POST with Content-Length smaller than actual body → 400 Bad Request
    """
    target = "/upload_store/cl_mismatch.txt"
    ensure_absent(target)

    raw = (
        f"POST {target} HTTP/1.1\r\n"
        f"Host: {urlparse(SERVER).hostname}\r\n"
        "Connection: close\r\n"
        "Content-Length: 5\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "TOO_LONG_CONTENT"
    )
    status, reason, _ = send_raw(raw)
    if status != 400:
        print(f"❌ RAW POST CL mismatch {target} → {status} (expected 400)")
        sys.exit(1)
    print("✅ RAW POST CL mismatch → 400 Bad Request")

def test_percent_encoded_traversal_varied():
    """
    POST to /upload_store/%2E%2E/outside2.txt → 403 Forbidden
    """
    target = "/upload_store/%2E%2E/outside2.txt"
    local = os.path.normpath(os.path.join(UPLOAD_DIR, "../outside2.txt"))
    with open(local, "w") as f:
        f.write("stay intact")

    status, reason, _ = request(
        "POST",
        target,
        body="attempt",
        headers={"Content-Type": "text/plain"},
        expected=403
    )
    if not os.path.isfile(local):
        print(f"❌ {local} was modified during traversal test")
        sys.exit(1)
    print("✅ Percent-encoded traversal → 403 and outside file intact")

    os.remove(local)

def test_absolute_uri_request_line():
    """
    POST with absolute URI in request-line → 400 Bad Request
    """
    target = "/upload_store/absuri.txt"
    ensure_absent(target)

    host = urlparse(SERVER).hostname
    raw = (
        f"POST http://{host}:{urlparse(SERVER).port}{target} HTTP/1.1\r\n"
        f"Host: {host}\r\n"
        "Connection: close\r\n"
        "Content-Length: 3\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "bad"
    )
    status, reason, _ = send_raw(raw)
    if status != 400:
        print(f"❌ RAW POST absolute URI → {status} (expected 400)")
        sys.exit(1)
    print("✅ RAW POST absolute URI → 400 Bad Request")

def test_content_disposition_unicode_filename():
    """
    POST multipart with unicode filename → 201 Created
    """
    filename = "测试.txt"
    # Percent-encode for URI
    target = f"/upload_store/{filename}"
    uri = "/upload_store/" + "%E6%B5%8B%E8%AF%95.txt"
    ensure_absent(f"/upload_store/{filename}")

    boundary = "BOUNDARYUNICODE"
    text_part = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="file"; filename="{filename}"\r\n'
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "\r\n"
        "Unicode content\r\n"
        f"--{boundary}--\r\n"
    )
    body_bytes = text_part.encode("utf-8")
    headers = {
        "Content-Type": f"multipart/form-data; boundary={boundary}",
        "Content-Length": str(len(body_bytes))
    }

    status, reason, _ = request(
        "POST",
        "/upload_store/",
        body=body_bytes,
        headers=headers,
        expected=201
    )

    parsed = urlparse(SERVER)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", uri)
    res  = conn.getresponse()
    body = res.read().decode(errors="replace")
    conn.close()
    if res.status != 200 or "Unicode content" not in body:
        print(f"❌ GET unicode file → {res.status} or body mismatch")
        sys.exit(1)
    print("✅ Unicode multipart upload → 201 and content verified")

    request("DELETE", uri, expected=200)

def test_multipart_mixed_fields_and_files():
    """
    POST multipart with both text fields and file → 201 Created, file part written
    """
    filename = "mixed.txt"
    target = "/upload_store/mixed.txt"
    ensure_absent(target)

    boundary = "MIXEDBOUNDARY"
    multipart_body = (
        f"--{boundary}\r\n"
        "Content-Disposition: form-data; name=\"field1\"\r\n"
        "\r\n"
        "value1\r\n"
        f"--{boundary}\r\n"
        f"Content-Disposition: form-data; name=\"file\"; filename=\"{filename}\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Mixed content\r\n"
        f"--{boundary}--\r\n"
    )

    status, reason, _ = request(
        "POST",
        "/upload_store/",
        body=multipart_body,
        headers={"Content-Type": f"multipart/form-data; boundary={boundary}"},
        expected=201
    )

    parsed = urlparse(SERVER)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", target)
    res  = conn.getresponse()
    body = res.read().decode(errors="replace")
    conn.close()
    if res.status != 200 or "Mixed content" not in body:
        print(f"❌ GET mixed file → {res.status} or body mismatch")
        sys.exit(1)
    print("✅ Multipart mixed fields/files → 201 and file content verified")

    request("DELETE", target, expected=200)
    
def send_multipart(body: bytes, boundary: str, expected_status: int):
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    headers = {
        "Content-Type": f"multipart/form-data; boundary={boundary}",
        "Content-Length": str(len(body))
    }
    conn.request("POST", "/upload_store/", body=body, headers=headers)
    res = conn.getresponse()
    data = res.read()
    conn.close()
    if res.status != expected_status:
        print(f"❌ Expected {expected_status}, got {res.status}")
        print(data.decode(errors="replace"))
        sys.exit(1)
    else:
        print(f"✅ {expected_status} as expected")

def test_all_multipart_variants():
    boundary = "MINIMAL_TEST"

    # 1) Single file part only
    body1 = (
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="file"; filename="one.txt"\r\n'
        "Content-Type: text/plain\r\n"
        "\r\n"
        "FileOneContent\r\n"
        f"--{boundary}--\r\n"
    ).encode("utf-8")
    send_multipart(body1, boundary, expected_status=201)

    # 2) Single text field only (should be 400, no file)
    body2 = (
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="field_only"\r\n'
        "\r\n"
        "value\r\n"
        f"--{boundary}--\r\n"
    ).encode("utf-8")
    send_multipart(body2, boundary, expected_status=400)

    # 3) Field first, then file
    body3 = (
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="field1"\r\n'
        "\r\n"
        "value1\r\n"
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="file"; filename="two.txt"\r\n'
        "Content-Type: text/plain\r\n"
        "\r\n"
        "FileTwoContent\r\n"
        f"--{boundary}--\r\n"
    ).encode("utf-8")
    send_multipart(body3, boundary, expected_status=201)

    # 4) File first, then field next (should ignore field and still succeed)
    body4 = (
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="file"; filename="three.txt"\r\n'
        "Content-Type: text/plain\r\n"
        "\r\n"
        "FileThree\r\n"
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="desc"\r\n'
        "\r\n"
        "extra\r\n"
        f"--{boundary}--\r\n"
    ).encode("utf-8")
    send_multipart(body4, boundary, expected_status=201)

    # 5) Multiple file parts (only first should be saved)
    body5 = (
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="file"; filename="first.txt"\r\n'
        "Content-Type: text/plain\r\n"
        "\r\n"
        "FirstFile\r\n"
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="file2"; filename="second.txt"\r\n'
        "Content-Type: text/plain\r\n"
        "\r\n"
        "SecondFile\r\n"
        f"--{boundary}--\r\n"
    ).encode("utf-8")
    send_multipart(body5, boundary, expected_status=201)

    print("✅ All multipart variants passed.")

def test_http10_post():
    """
    POST using HTTP/1.0 → 201 Created
    """
    target = "/upload_store/http10.txt"
    ensure_absent(target)

    raw = (
        f"POST {target} HTTP/1.0\r\n"
        f"Host: {urlparse(SERVER).hostname}\r\n"
        "Connection: close\r\n"
        "Content-Length: 12\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "HELLO_HTTP10"
    )
    status, reason, _ = send_raw(raw)
    if status != 201:
        print(f"❌ HTTP/1.0 POST {target} → {status} (expected 201)")
        sys.exit(1)
    print("✅ HTTP/1.0 POST → 201 Created")

    parsed = urlparse(SERVER)
    conn   = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("GET", target)
    res  = conn.getresponse()
    body = res.read().decode(errors="replace")
    conn.close()
    if res.status != 200 or "HELLO_HTTP10" not in body:
        print(f"❌ GET http10.txt → {res.status} or body mismatch")
        sys.exit(1)
    print("✅ GET http10.txt → 200 OK, content verified")

    request("DELETE", target, expected=200)

def run_tests():
    print("[ POST Test Suite ]")

    test_raw_body_upload()
    test_urlencoded_form_upload()
    test_multipart_form_upload()
    test_missing_content_type()
    test_upload_to_non_upload_location()
    test_path_traversal_upload()
    test_overwrite_existing()
    test_percent_encoded_filename()
    test_nested_directory_upload()
    test_long_url_too_long()

    test_chunked_transfer_encoding()
    test_malformed_chunked_encoding()
    test_missing_final_chunk()
    test_expect_100_continue()
    test_content_length_and_chunked_conflict()
    test_conflicting_content_length_headers()
    test_empty_body_no_content_length()
    test_multipart_missing_boundary()
    test_unsupported_media_type_xml()
    test_line_ending_lf_only()
    test_header_folding_rejection()
    test_max_body_size_content_length()
    test_max_body_size_chunked()
    test_content_length_mismatch()
    test_percent_encoded_traversal_varied()
    test_absolute_uri_request_line()
    test_content_disposition_unicode_filename()
    test_multipart_mixed_fields_and_files()
    test_all_multipart_variants()
    test_http10_post()

if __name__ == "__main__":
    run_tests()
