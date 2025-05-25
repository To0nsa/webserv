import socket
import time

HOST = "127.0.0.1"
PORT = 8001

def send_and_recv(sock, data, delay=0):
    sock.sendall(data.encode())
    if delay > 0:
        time.sleep(delay)
    try:
        sock.settimeout(2)
        resp = sock.recv(8192).decode(errors='replace')
        print("----- Response Start -----")
        print(resp)
        print("----- Response End -----\n")
    except socket.timeout:
        print("⏱ No response (timeout)")

def test_normal_post(sock):
    print("🔹 Normal POST")
    send_and_recv(sock, (
        "POST /post_body HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n\r\n"
        "hello"
    ))

def test_pipelined(sock):
    print("🔹 Pipelined POST + GET")
    send_and_recv(sock, (
        "POST /post_body HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n\r\n"
        "hello"
        "GET /directory/ HTTP/1.1\r\n"
        "Host: localhost\r\n\r\n"
    ))

def test_slow_body(sock):
    print("🔹 Slow-delivered body")
    sock.sendall((
        "POST /post_body HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n\r\n"
        "he"
    ).encode())
    time.sleep(1)
    sock.sendall("llo".encode())
    time.sleep(1)
    try:
        resp = sock.recv(8192).decode(errors='replace')
        print("----- Response Start -----")
        print(resp)
        print("----- Response End -----\n")
    except socket.timeout:
        print("⏱ No response (timeout)")

def test_chunked(sock):
    print("🔹 Chunked POST")
    send_and_recv(sock, (
        "POST /post_body HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
        "5\r\nhello\r\n0\r\n\r\n"
    ))

def test_chunked_slow(sock):
    print("🔹 Chunked POST (slow chunks)")
    sock.sendall((
        "POST /post_body HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n\r\n"
    ).encode())
    time.sleep(0.5)
    sock.sendall("5\r\nhel".encode())
    time.sleep(0.5)
    sock.sendall("lo\r\n0\r\n\r\n".encode())
    try:
        resp = sock.recv(8192).decode(errors='replace')
        print("----- Response Start -----")
        print(resp)
        print("----- Response End -----\n")
    except socket.timeout:
        print("⏱ No response (timeout)")

def test_empty_post(sock):
    print("🔹 Empty POST")
    send_and_recv(sock, (
        "POST /post_body HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 0\r\n\r\n"
    ))

def test_malformed(sock):
    print("🔹 Malformed request")
    send_and_recv(sock, (
        "POST /post_body HTTP/1.1\n"
        "Content-Length: 5\n"
        "\n"
        "hello"
    ))  # missing \r

def test_overlapping(sock):
    print("🔹 Overlapping pipelined requests (invalid)")
    send_and_recv(sock, (
        "POST /post_body HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 10\r\n\r\n"
        "helloPOST /post_body HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n\r\n"
        "world"
    ))

def test_oversized(sock):
    print("🔹 Oversized POST")
    payload = "A" * (1024 * 1024 * 5)  # 5MB
    send_and_recv(sock, (
        "POST /post_body HTTP/1.1\r\n"
        "Host: localhost\r\n"
        f"Content-Length: {len(payload)}\r\n\r\n"
    ) + payload[:200])  # only send partial data to simulate early disconnect

def run_all():
    tests = [
        test_normal_post,
        test_pipelined,
        test_slow_body,
        test_chunked,
        test_chunked_slow,
        test_empty_post,
        test_malformed,
        test_overlapping,
        test_oversized,
    ]

    for test_func in tests:
        print("="*60)
        with socket.create_connection((HOST, PORT)) as sock:
            test_func(sock)
        print("="*60 + "\n")
        time.sleep(1)

if __name__ == "__main__":
    run_all()
