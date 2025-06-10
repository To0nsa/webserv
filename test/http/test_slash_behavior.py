import http.client
import os
import sys
from urllib.parse import urlparse

# Base URL of your running server (must point at the vhost on port 8080)
SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")

def setup_test_dir():
    """Create test/data/all with an index.html and a simple CGI script."""
    data_dir = "test/data/all"
    os.makedirs(data_dir, exist_ok=True)

    # index.html for GET /all/
    with open(os.path.join(data_dir, "index.html"), "w") as f:
        f.write("<html><body><h1>Index of /all/</h1></body></html>")

    # Simple CGI script hello.py
    cgi = os.path.join(data_dir, "hello.py")
    with open(cgi, "w") as f:
        f.write("#!/usr/bin/env python3\n")
        f.write("print('Content-Type: text/plain')\n")
        f.write("print()\n")
        f.write("print('Hello from CGI')\n")
    os.chmod(cgi, 0o755)

def request(method, path, body=None, headers=None):
    """Helper: send a request and return (status, location_header, body)."""
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)

    payload = body or ""
    hdrs = (headers or {}).copy()

    if method == "POST":
        # ensure a non-empty body + content-type
        if not payload:
            payload = "x"
        hdrs["Content-Type"] = "application/octet-stream"
        hdrs["Content-Length"] = str(len(payload))
    elif method == "DELETE":
        hdrs["Content-Length"] = "0"

    conn.request(method, path, body=payload, headers=hdrs)
    res = conn.getresponse()
    loc = res.getheader("Location")
    data = res.read().decode(errors="replace")
    conn.close()
    return res.status, loc, data

def assert_redirect(path, expected_loc):
    status, loc, _ = request("GET", path)
    if status == 301 and loc == expected_loc:
        print(f"✅ GET {path} → 301 Location: {loc}")
    else:
        print(f"❌ GET {path} → {status} (Location: {loc!r}, expected {expected_loc!r})")
        sys.exit(1)

def assert_status(method, path, expected):
    status, _, _ = request(method, path)
    if status == expected:
        print(f"✅ {method} {path} → {status}")
    else:
        print(f"❌ {method} {path} → {status} (expected {expected})")
        sys.exit(1)

def test_directory_redirect():
    # GET without slash → 301; with slash → 200
    assert_redirect("/all", "/all/")
    assert_status("GET", "/all/", 200)

def test_non_get_methods_no_redirect_and_upload():
    # DELETE on directory → forbidden
    assert_status("DELETE", "/all", 403)
    assert_status("DELETE", "/all/", 403)
    # POST on directory → 201 Created (raw body upload)
    assert_status("POST", "/all", 201)
    assert_status("POST", "/all/", 201)

def test_cgi_invocation():
    # CGI via GET and POST
    st, _, body = request("GET", "/all/hello.py")
    if st == 200 and "Hello from CGI" in body:
        print("✅ GET /all/hello.py → CGI OK")
    else:
        print(f"❌ GET /all/hello.py → {st}")
        sys.exit(1)

    st, _, body = request("POST", "/all/hello.py", body="ignored")
    if st == 200 and "Hello from CGI" in body:
        print("✅ POST /all/hello.py → CGI OK")
    else:
        print(f"❌ POST /all/hello.py → {st}")
        sys.exit(1)

def test_trailing_slash_on_cgi_and_delete():
    # File paths with trailing slash → no redirect
    assert_status("GET",    "/all/hello.py/", 404)
    assert_status("POST",   "/all/hello.py/", 404)
    assert_status("DELETE", "/all/hello.py/", 403)

def test_delete_cgi_file():
    # DELETE the script → 200 then GET it → 404
    assert_status("DELETE", "/all/hello.py", 200)
    assert_status("GET",    "/all/hello.py", 404)

if __name__ == "__main__":
    print("\n[ Slash behavior Test Suite ]\n")
    setup_test_dir()
    test_directory_redirect()
    test_non_get_methods_no_redirect_and_upload()
    test_cgi_invocation()
    test_trailing_slash_on_cgi_and_delete()
    test_delete_cgi_file()
    print("\n✅ All /all-location tests passed.\n")
