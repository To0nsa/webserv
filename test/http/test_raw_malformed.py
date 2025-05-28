# test/test_raw_malformed.py

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

    # 1. Missing request-target (e.g., just GET and version)
    res = send_raw_request("GET  HTTP/1.1\r\nHost: localhost\r\n\r\n")
    assert_contains(res, "400 Bad Request", "Missing request-target")

    # 2. Invalid method casing
    res = send_raw_request("get / HTTP/1.1\r\nHost: localhost\r\n\r\n")
    assert_contains(res, "400 Bad Request", "Invalid method casing")

    # 3. No HTTP version
    res = send_raw_request("GET / \r\nHost: localhost\r\n\r\n")
    assert_contains(res, "400 Bad Request", "Missing HTTP version")

    # 4. Missing Host header (HTTP/1.1)
    res = send_raw_request("GET / HTTP/1.1\r\n\r\n")
    assert_contains(res, "400 Bad Request", "Missing Host header")

    # 5. Valid HTTP/1.0 request without Host
    res = send_raw_request("GET / HTTP/1.0\r\n\r\n")
    assert_contains(res, "200 OK", "Valid HTTP/1.0 request")

    print("[RAW] ✅ All raw tests passed.")


if __name__ == "__main__":
    run_raw_tests()
