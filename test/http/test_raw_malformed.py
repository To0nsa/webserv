import socket
import sys
import os

HOST = os.getenv("WEBSERV_HOST", "127.0.0.1")
PORT = int(os.getenv("WEBSERV_PORT", "8080"))


def send_raw_request(raw):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((HOST, PORT))
        s.sendall(raw.encode())
        response = s.recv(4096).decode(errors="replace")
    return response


def assert_contains(response, expected_status, context):
    if expected_status in response:
        print(f"✅ {context} → {expected_status}")
    else:
        print(f"❌ {context} → did not get expected {expected_status}")
        print("---- Response ----")
        print(response)
        print("------------------")
        sys.exit(1)


def run_raw_tests():
    print("[RAW] Running malformed/raw request tests...")

    tests = [
        ("GET  HTTP/1.1\r\nHost: localhost\r\n\r\n",         "400 Bad Request", "Missing request-target"),
        ("GET / \r\nHost: localhost\r\n\r\n",                "400 Bad Request", "Missing HTTP version"),
        ("GET / HTTP/1.1\r\n\r\n",                           "400 Bad Request", "Missing Host header"),
        ("GET / HTTP/1.0\r\n\r\n",                           "200 OK",          "Valid HTTP/1.0 request"),
        ("GET   /index.html   HTTP/1.1\r\nHost: localhost\r\n\r\n", "400 Bad Request", "Extra spaces in request line"),
        ("G@T /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n",     "400 Bad Request", "Invalid characters in method"),
        ("GET /he\x01llo HTTP/1.1\r\nHost: localhost\r\n\r\n",     "400 Bad Request", "Control character in path"),
        ("GET / HTTP/1.1\r\nHost: localhost\r\nHost: evil.com\r\n\r\n", "400 Bad Request", "Duplicate Host headers"),
        ("GET / HTTP/0.9\r\n\r\n",                             "505 HTTP Version Not Supported", "Unsupported HTTP version"),
        ("GET / HTTP/1.1\r\n\r\n\r\n\r\n",                     "400 Bad Request", "Too many CRLF after headers"),
        ("get / HTTP/1.1\r\nHost: localhost\r\n\r\n",          "501 Not Implemented", "Invalid method casing"),
        ("GET / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n4\r\ntest\r\n0\r\n\r\n", "400 Bad Request", "GET with chunked body"),
        ("GET / HTTP/1.1\r\nHost: localhost\r\nX-Header:\r\n\r\n", "400 Bad Request", "Header with missing value"),
        ("GET / HTTP/1.1\r\nHost: localhost\r\nX-BAD:\r\n\r\n", "400 Bad Request", "Header with missing value"),
        ("CONNECT / HTTP/1.1\r\nHost: localhost\r\n\r\n", "501 Not Implemented", "Unsupported CONNECT method"),
        ("GET / HTTP/1.1\r\nHost: localhost\r\nX-Test: val1\r\nX-Test: val2\r\n\r\n", "200 OK", "Duplicate but mergeable headers"),
        ("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nContent-Length: 10\r\n\r\nhello", "400 Bad Request", "Duplicate Content-Length headers"),
        ("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n4\r\ntest\r\n0\r\n\r\n", "400 Bad Request", "Conflicting Content-Length and Transfer-Encoding"),
        ("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\n\r\nhello", "411 Length Required", "Non-numeric Content-Length"),
        ("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: -42\r\n\r\nhello", "411 Length Required", "Negative Content-Length"),
        ("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nContent-Type: text/plain\r\nContent-Type: application/json\r\n\r\nhello", "400 Bad Request", "Duplicate Content-Type headers"),
        (
            "POST / HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Length: 5\r\n"
            "Content-Type: application/x-evil\r\n"
            "\r\n"
            "hello",
            "415 Unsupported Media Type",
            "Unsupported Content-Type"
        ),
        ("POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip\r\n\r\nhello", "501 Not Implemented", "Unsupported Transfer-Encoding value"),
        ("GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nConnection: keep-alive\r\n\r\n", "400 Bad Request", "Conflicting Connection header values"),
        ("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nExpect: 100-continue\r\n\r\nhello", "417 Expectation Failed", "Unsupported Expect header"),
        ("GET / HTTP/1.1\r\nHost localhost\r\n\r\n", "400 Bad Request", "Header without colon"),
        ("GET / HTTP/1.1\r\nHo\x01st: localhost\r\n\r\n", "400 Bad Request", "Control char in header name"),
        ("GET / HTTP/1.1\r\nHost : localhost\r\n\r\n", "400 Bad Request", "Space in header name"),
        ("GET / HTTP/1.1\r\nHost:\tlocalhost\r\n\r\n", "200 OK", "Tab in header value (legal but rare)"),
        ("GET / HTTP/1.1\r\nHost: localhost\r\nX-Folded: hello\r\n world\r\n\r\n", "400 Bad Request", "Obsolete folded header line"),
        ("GET /" + "a"*3000 + " HTTP/1.1\r\nHost: localhost\r\n\r\n", "414 Request-URI Too Long", "Too long URI"),
        ("PoSt / HTTP/1.1\r\nHost: localhost\r\n\r\n", "501 Not Implemented", "Mixed-case HTTP method"),
        ("POST / HTTP/1.1\r\nHost: localhost\r\n\r\nhello", "411 Length Required", "POST with no Content-Length or TE"),
        ("POST / HTTP/1.1\r\nHost: localhost\r\nExpect: 100-continue\r\nExpect: 100-continue\r\n\r\nhello", "417 Expectation Failed", "Duplicate Expect headers"),
        ("GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close, keep-alive\r\n\r\n", "400 Bad Request", "Conflicting Connection header values in single line"),
    ]

    for raw, expected, context in tests:
        res = send_raw_request(raw)
        assert_contains(res, expected, context)


if __name__ == "__main__":
    run_raw_tests()

