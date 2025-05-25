import socket
import time
import os

HOST = "127.0.0.1"
PORT = 8001
PASS = []

def log(title, passed):
    symbol = "✔️" if passed else "❌"
    color = "\033[92m" if passed else "\033[91m"
    print(f"{color}{symbol} {title}\033[0m")
    PASS.append(passed)

def send_and_recv(sock, data, delay=0, expect=None):
    sock.sendall(data.encode())
    if delay > 0:
        time.sleep(delay)
    try:
        sock.settimeout(2)
        resp = sock.recv(8192).decode(errors='replace')
        print("----- Response Start -----")
        print(resp)
        print("----- Response End -----\n")
        if expect and expect not in resp:
            return False
        return True
    except socket.timeout:
        print("⏱ No response (timeout)")
        return False

def test_normal_post(sock):
    print("🔹 Normal POST")
    ok = send_and_recv(sock,
        "POST /post_body HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello",
        expect="201 Created"
    )
    log("Normal POST", ok)

def test_get(sock):
    print("🔹 GET /directory/")
    ok = send_and_recv(sock,
        "GET /directory/ HTTP/1.1\r\nHost: localhost\r\n\r\n",
        expect="200 OK"
    )
    log("GET /directory/", ok)

def test_delete(sock):
    print("🔹 DELETE /uploads/upload_test.txt")
    # Ensure file exists
    path = "/home/toonsa/myProjects/webserv/serverfiles/uploads/upload_test.txt"
    with open(path, "w") as f:
        f.write("delete me")
    ok = send_and_recv(sock,
        "DELETE /uploads/upload_test.txt HTTP/1.1\r\nHost: localhost\r\n\r\n",
        expect="200"
    )
    log("DELETE existing file", ok)

def test_post_cgi(sock):
    print("🔹 POST CGI .py")
    ok = send_and_recv(sock,
        "POST /cgi-bin/hello.py HTTP/1.1\r\nHost: localhost\r\nContent-Length: 11\r\n\r\nhello=cgi!",
        expect="200 OK"
    )
    log("POST CGI .py", ok)

def test_pipelined(sock):
    print("🔹 Pipelined POST + GET")
    ok = send_and_recv(sock,
        "POST /post_body HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello"
        "GET /directory/ HTTP/1.1\r\nHost: localhost\r\n\r\n",
        expect="400 Bad Request"
    )
    log("Pipelined POST + GET", ok)

def test_slow_body(sock):
    print("🔹 Slow-delivered body")
    sock.sendall((
        "POST /post_body HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhe"
    ).encode())
    time.sleep(1)
    sock.sendall("llo".encode())
    time.sleep(1)
    try:
        resp = sock.recv(8192).decode(errors='replace')
        print("----- Response Start -----")
        print(resp)
        print("----- Response End -----\n")
        passed = "201 Created" in resp
    except socket.timeout:
        passed = False
    log("Slow POST body", passed)

def test_chunked(sock):
    print("🔹 Chunked POST")
    ok = send_and_recv(sock,
        "POST /post_body HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n",
        expect="201 Created"
    )
    log("Chunked POST", ok)

def test_chunked_slow(sock):
    print("🔹 Chunked POST (slow)")
    sock.sendall("POST /post_body HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n".encode())
    time.sleep(0.5)
    sock.sendall("5\r\nhel".encode())
    time.sleep(0.5)
    sock.sendall("lo\r\n0\r\n\r\n".encode())
    try:
        resp = sock.recv(8192).decode(errors='replace')
        print("----- Response Start -----")
        print(resp)
        print("----- Response End -----\n")
        passed = "201 Created" in resp
    except socket.timeout:
        passed = False
    log("Slow Chunked POST", passed)

def test_empty_post(sock):
    print("🔹 Empty POST")
    ok = send_and_recv(sock,
        "POST /post_body HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n",
        expect="201 Created"
    )
    log("Empty POST", ok)

def test_malformed(sock):
    print("🔹 Malformed request")
    ok = send_and_recv(sock,
        "POST /post_body HTTP/1.1\nContent-Length: 5\n\nhello",
        expect="400 Bad Request"
    )
    log("Malformed headers", ok)

def test_overlapping(sock):
    print("🔹 Overlapping pipelined POST")
    ok = send_and_recv(sock,
        "POST /post_body HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\n"
        "helloPOST /post_body HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nworld",
        expect="400 Bad Request"
    )
    log("Overlapping pipelined POSTs", ok)

def test_oversized(sock):
    print("🔹 Oversized POST (incomplete)")
    payload = "A" * (1024 * 1024 * 5)  # 5MB
    ok = send_and_recv(sock,
        "POST /post_body HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5242880\r\n\r\n" + payload[:200]
    )
    log("Oversized POST (should timeout or fail safely)", not ok)  # Expect no response

def run_all():
    tests = [
        test_normal_post,
        test_get,
        test_delete,
        test_post_cgi,
        test_pipelined,
        test_slow_body,
        test_chunked,
        test_chunked_slow,
        test_empty_post,
        test_malformed,
        test_overlapping,
        test_oversized,
    ]
    for test in tests:
        print("="*60)
        with socket.create_connection((HOST, PORT)) as sock:
            test(sock)
        print("="*60 + "\n")
        time.sleep(0.5)

    print("\n=========== TEST SUMMARY ===========")
    total = len(PASS)
    passed = sum(PASS)
    color = "\033[92m" if passed == total else "\033[91m"
    print(f"{color}{passed}/{total} tests passed.\033[0m")
    print("====================================")

if __name__ == "__main__":
    run_all()
