import socket
import re

# ANSI color codes
RED = "\033[91m"
GREEN = "\033[92m"
RESET = "\033[0m"

def send_request(request: str, host='127.0.0.1', port=8001):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((host, port))
        sock.sendall(request.encode())
        response = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk
        return response.decode(errors='ignore')

def extract_status_code(response: str):
    match = re.match(r"HTTP/\d+\.\d+ (\d+)", response)
    return int(match.group(1)) if match else None

# Define test cases
tests = [
    {
        "name": "Simple GET request",
        "request": "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n",
        "expected": 200
    },
    {
        "name": "GET /.. path (possible directory traversal)",
        "request": "GET /.. HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n",
        "expected": 403  # or 400 if you treat it as malformed
    },
    {
        "name": "Invalid header (no colon)",
        "request": "GET / HTTP/1.1\r\nHost localhost\r\nConnection: close\r\n\r\n",
        "expected": 400
    },
    {
        "name": "Invalid Content-Type header",
        "request": (
            "POST /submit HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Type: =/invalid\r\n"
            "Connection: close\r\n"
            "Content-Length: 13\r\n\r\n"
            "name=test"
        ),
        "expected": 400  # or 415 depending on parser behavior
    },
    {
        "name": "Malformed request (missing HTTP version)",
        "request": "GET /missing-version\r\nHost: localhost\r\nConnection: close\r\n\r\n",
        "expected": 400
    },   
    {
        "name": "Missing Host header (HTTP/1.0 doesn't require it)",
        "request": "GET / HTTP/1.0\r\nConnection: close\r\n\r\n",
        "expected": 200
    },
    {
        "name": "Missing Host header (HTTP/1.1 requires it)",
        "request": "GET / HTTP/1.1\r\nConnection: close\r\n\r\n",
        "expected": 400
    },
    {
        "name": "Invalid Host header (HTTP/1.4)",
        "request": "GET / HTTP/1.4\r\nHost: localhost\r\nConnection: close\r\n\r\n",
        "expected": 505  # HTTP Version Not Supported
    },
    {
        "name": "Invalid Method header (GETt)",
        "request": "GETt / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n",
        "expected": 400
    },
    {
        "name": "Invalid Connection header (Connection: WRONG)",
        "request": "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: WRONG\r\n\r\n",
        "expected": 400
    }
]

# Run tests
for test in tests:
    print(f"=== {test['name']} ===")
    response = send_request(test['request'])
    actual_code = extract_status_code(response)
    if actual_code == test['expected']:
        result = f"{GREEN}PASS{RESET}"
    else:
        result = f"{RED}FAIL (expected {test['expected']}, got {actual_code}){RESET}"
    print(f"Status: {actual_code} → {result}")
    print(response)
    print("\n" + "="*50 + "\n")
