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
        # Syntax errors
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
        ("GET / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n4\r\ntest\r\n0\r\n\r\n", "400 Bad Request", "GET with chunked body")
    ]

    for raw, expected, context in tests:
        res = send_raw_request(raw)
        assert_contains(res, expected, context)

    print("[RAW] ✅ All raw tests passed.")


if __name__ == "__main__":
    run_raw_tests()

