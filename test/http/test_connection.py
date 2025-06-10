#!/usr/bin/env python3
import os
import sys
import socket
from urllib.parse import urlparse

# Server URL (env WEBSERV_URL or default)
SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")


def send_raw(request_str):
    """
    Send a single HTTP request over TCP and read headers + body based on Content-Length.
    """
    parsed = urlparse(SERVER)
    host, port = parsed.hostname, parsed.port
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
    status_code = int(parts[1]) if len(parts) > 1 else 0
    reason = parts[2] if len(parts) > 2 else ""
    raw = (header_part + b"\r\n\r\n" + body).decode(errors="replace")
    return status_code, reason, raw


def send_pipeline(requests_str):
    """
    Send multiple HTTP requests in one TCP write, then read until timeout and return raw response.
    """
    parsed = urlparse(SERVER)
    host, port = parsed.hostname, parsed.port
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    sock.sendall(requests_str.encode())
    sock.settimeout(1.0)
    data = b""
    try:
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    finally:
        sock.close()
    return data.decode(errors="replace")


def test_connection_keep_alive_header():
    """
    Single GET/1.1 should return Connection: keep-alive
    """
    host = urlparse(SERVER).hostname
    req = (
        f"GET /nonexistent HTTP/1.1\r\n"
        f"Host: {host}\r\n"
        "\r\n"
    )
    status, reason, raw = send_raw(req)
    if status != 404 or "Connection: keep-alive" not in raw:
        print(f"❌ test_connection_keep_alive_header → {status} {reason}\n{raw}")
        sys.exit(1)
    print("✅ test_connection_keep_alive_header → got Connection: keep-alive")


def test_connection_close_header():
    """
    Single GET/1.1 + Connection: close should return Connection: close
    """
    host = urlparse(SERVER).hostname
    req = (
        f"GET /nonexistent HTTP/1.1\r\n"
        f"Host: {host}\r\n"
        "Connection: close\r\n"
        "\r\n"
    )
    status, reason, raw = send_raw(req)
    if status != 404 or "Connection: close" not in raw:
        print(f"❌ test_connection_close_header → {status} {reason}\n{raw}")
        sys.exit(1)
    print("✅ test_connection_close_header → got Connection: close")


def test_pipeline_keep_alive():
    """
    Two GETs in one write → expect at least two 404 status lines and two keep-alive headers
    """
    host = urlparse(SERVER).hostname
    pipeline = (
        f"GET /a HTTP/1.1\r\nHost: {host}\r\n\r\n"
        f"GET /b HTTP/1.1\r\nHost: {host}\r\n\r\n"
    )
    raw = send_pipeline(pipeline)
    count_404 = raw.count("HTTP/1.1 404")
    count_ka = raw.lower().count("connection: keep-alive")
    if count_404 < 2 or count_ka < 2:
        print(f"❌ test_pipeline_keep_alive: found {count_404} 404 lines and {count_ka} keep-alive headers\n{raw}")
        sys.exit(1)
    print("✅ test_pipeline_keep_alive")


def test_pipeline_close_first():
    """
    First request has Connection: close → only one response, then drop
    """
    host = urlparse(SERVER).hostname
    pipeline = (
        f"GET /x HTTP/1.1\r\nHost: {host}\r\nConnection: close\r\n\r\n"
        f"GET /y HTTP/1.1\r\nHost: {host}\r\n\r\n"
    )
    raw = send_pipeline(pipeline)
    count = raw.count("HTTP/1.1")
    if count != 1 or "Connection: close" not in raw:
        print(f"❌ test_pipeline_close_first: expected 1 response with connection close, got {count}\n{raw}")
        sys.exit(1)
    print("✅ test_pipeline_close_first")


def test_pipeline_close_middle():
    """
    Second request has Connection: close → two responses, then drop
    """
    host = urlparse(SERVER).hostname
    pipeline = (
        f"GET /m1 HTTP/1.1\r\nHost: {host}\r\n\r\n"
        f"GET /m2 HTTP/1.1\r\nHost: {host}\r\nConnection: close\r\n\r\n"
    )
    raw = send_pipeline(pipeline)
    count = raw.count("HTTP/1.1")
    if count < 2:
        print(f"❌ test_pipeline_close_middle: expected at least 2 responses, got {count}\n{raw}")
        sys.exit(1)
    second_part = raw.split("HTTP/1.1")[2]
    if "connection: close" not in second_part.lower():
        print(f"❌ test_pipeline_close_middle: second response missing connection: close\n{raw}")
        sys.exit(1)
    print("✅ test_pipeline_close_middle")


def test_pipeline_error400_closes():
    """
    Malformed request first → 400 Bad Request and connection closed, no second
    """
    pipeline = (
        "BADCMD\r\n\r\n"
        f"GET /shouldnot HTTP/1.1\r\nHost: {urlparse(SERVER).hostname}\r\n\r\n"
    )
    raw = send_pipeline(pipeline)
    if not raw.startswith("HTTP/1.1 400") or raw.count("HTTP/1.1") > 1:
        print(f"❌ test_pipeline_error400_closes: unexpected response(s)\n{raw}")
        sys.exit(1)
    print("✅ test_pipeline_error400_closes")


def test_pipeline_error413_closes():
    """
    Oversized POST first → 413 Payload Too Large and connection closed
    """
    host = urlparse(SERVER).hostname
    pipeline = (
        f"POST /upload_store/large_cl.txt HTTP/1.1\r\n"
        f"Host: {host}\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 999999999\r\n"
        "\r\n"
        f"GET /next HTTP/1.1\r\nHost: {host}\r\n\r\n"
    )
    raw = send_pipeline(pipeline)
    if not raw.startswith("HTTP/1.1 413") or raw.count("HTTP/1.1") > 1:
        print(f"❌ test_pipeline_error413_closes: unexpected response(s)\n{raw}")
        sys.exit(1)
    print("✅ test_pipeline_error413_closes")
    
def send_raw(request_str):
    parsed = urlparse(SERVER)
    host, port = parsed.hostname, parsed.port
    sock = socket.socket()
    sock.connect((host, port))
    sock.sendall(request_str.encode())
    response = b""
    while b"\r\n\r\n" not in response:
        chunk = sock.recv(4096)
        if not chunk:
            break
        response += chunk
    header_part, rest = response.split(b"\r\n\r\n", 1)
    headers = header_part.decode().split("\r\n")[1:]
    length = 0
    for h in headers:
        if h.lower().startswith("content-length:"):
            length = int(h.split(":",1)[1].strip())
            break
    body = rest
    to_read = length - len(rest)
    while to_read > 0:
        chunk = sock.recv(4096)
        if not chunk:
            break
        body += chunk
        to_read -= len(chunk)
    sock.close()
    status_line = header_part.split(b"\r\n",1)[0].decode()
    parts = status_line.split(" ", 2)
    code = int(parts[1]) if len(parts) > 1 else 0
    reason = parts[2] if len(parts) > 2 else ""
    raw = (header_part + b"\r\n\r\n" + body).decode(errors="replace")
    return code, reason, raw


def send_pipeline(requests_str):
    parsed = urlparse(SERVER)
    host, port = parsed.hostname, parsed.port
    sock = socket.socket()
    sock.connect((host, port))
    sock.sendall(requests_str.encode())
    sock.settimeout(1.0)
    data = b""
    try:
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    finally:
        sock.close()
    return data.decode(errors="replace")

# existing tests omitted for brevity...

def test_pipeline_error413_closes():
    """
    Oversized POST → 413 Payload Too Large, Connection: close, no second response.
    """
    host = urlparse(SERVER).hostname
    pipeline = (
        f"POST /upload_store/large_cl.txt HTTP/1.1\r\n"
        f"Host: {host}\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 999999999\r\n"
        "\r\n"
        f"GET /next HTTP/1.1\r\nHost: {host}\r\n\r\n"
    )
    raw = send_pipeline(pipeline)
    if not raw.startswith("HTTP/1.1 413"):
        print(f"❌ test_pipeline_error413_closes: didn't start with 413\n{raw}")
        sys.exit(1)
    if raw.count("HTTP/1.1") > 1:
        print(f"❌ test_pipeline_error413_closes: saw multiple responses\n{raw}")
        sys.exit(1)
    if "connection: close" not in raw.lower():
        print(f"❌ test_pipeline_error413_closes: missing Connection: close header\n{raw}")
        sys.exit(1)
    print("✅ test_pipeline_error413_closes")


def test_pipeline_options_405_continues():
    """
    OPTIONS → 405, then GET → 404, Connection: keep-alive on both.
    """
    host = urlparse(SERVER).hostname
    pipeline = (
        f"OPTIONS / HTTP/1.1\r\nHost: {host}\r\n\r\n"
        f"GET /nonexistent HTTP/1.1\r\nHost: {host}\r\n\r\n"
    )
    raw = send_pipeline(pipeline)
    if raw.count("HTTP/1.1 405") != 1 or raw.count("HTTP/1.1 404") != 1:
        print(f"❌ test_pipeline_options_405_continues: wrong statuses\n{raw}")
        sys.exit(1)
    if raw.lower().count("connection: keep-alive") < 2:
        print(f"❌ test_pipeline_options_405_continues: missing keep-alive headers\n{raw}")
        sys.exit(1)
    print("✅ test_pipeline_options_405_continues")


def test_pipeline_trace_405_continues():
    """
    TRACE → 405, then GET → 404, Connection: keep-alive on both.
    """
    host = urlparse(SERVER).hostname
    pipeline = (
        f"TRACE / HTTP/1.1\r\nHost: {host}\r\n\r\n"
        f"GET /nonexistent HTTP/1.1\r\nHost: {host}\r\n\r\n"
    )
    raw = send_pipeline(pipeline)
    if raw.count("HTTP/1.1 405") != 1 or raw.count("HTTP/1.1 404") != 1:
        print(f"❌ test_pipeline_trace_405_continues: wrong statuses\n{raw}")
        sys.exit(1)
    if raw.lower().count("connection: keep-alive") < 2:
        print(f"❌ test_pipeline_trace_405_continues: missing keep-alive headers\n{raw}")
        sys.exit(1)
    print("✅ test_pipeline_trace_405_continues")
	
def test_pipeline_other_4xx_continues():
    """
    First request yields another 4xx (e.g. 405 Method Not Allowed),
    pipeline should still process the second GET and keep-alive.
    """
    host = urlparse(SERVER).hostname
    # Using an unsupported method to trigger 405
    pipeline = (
        f"OPTIONS / HTTP/1.1\r\nHost: {host}\r\n\r\n"
        f"GET /nonexistent HTTP/1.1\r\nHost: {host}\r\n\r\n"
    )
    raw = send_pipeline(pipeline)
    count_405 = raw.count("HTTP/1.1 405")
    count_404 = raw.count("HTTP/1.1 404")
    ka = "connection: keep-alive" in raw.lower()
    if count_405 != 1 or count_404 != 1 or not ka:
        print(f"❌ test_pipeline_other_4xx_continues: 405x{count_405}, 404x{count_404}, keep-alive? {ka}\n{raw}")
        sys.exit(1)
    print("✅ test_pipeline_other_4xx_continues")


def test_pipeline_5xx_continues():
    """
    First request yields a 5xx (e.g. 501 Not Implemented),
    pipeline should still process the second GET and keep-alive.
    """
    host = urlparse(SERVER).hostname
    # Using an unimplemented method to trigger 501 but use 405
    pipeline = (
        f"TRACE / HTTP/1.1\r\nHost: {host}\r\n\r\n"
        f"GET /nonexistent HTTP/1.1\r\nHost: {host}\r\n\r\n"
    )
    raw = send_pipeline(pipeline)
    count_405 = raw.count("HTTP/1.1 405")
    count_404 = raw.count("HTTP/1.1 404")
    ka = "connection: keep-alive" in raw.lower()
    if count_405 != 1 or count_404 != 1 or not ka:
        print(f"❌ test_pipeline_5xx_continues: 405x{count_405}, 404x{count_404}, keep-alive? {ka}\n{raw}")
        sys.exit(1)
    print("✅ test_pipeline_5xx_continues")


if __name__ == "__main__":
    test_connection_keep_alive_header()
    test_connection_close_header()
    test_pipeline_keep_alive()
    test_pipeline_close_first()
    test_pipeline_close_middle()
    test_pipeline_error400_closes()
    test_pipeline_error413_closes()
    test_pipeline_error413_closes()
    test_pipeline_options_405_continues()
    test_pipeline_trace_405_continues()
    test_pipeline_other_4xx_continues()
    test_pipeline_5xx_continues()
