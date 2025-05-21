import socket
import time

HOST = 'localhost'    # or '127.0.0.1'
PORT = 8001           # adjust to your server's port
PATH = '/uploads/test.txt'  # adjust route

BODY = "This is line one.\nThis is line two.\nAnd some more content.\n"
CONTENT_LENGTH = len(BODY)

HEADERS = (
    f"POST {PATH} HTTP/1.1\r\n"
    f"Host: {HOST}:{PORT}\r\n"
    f"Content-Type: text/plain\r\n"
    f"Content-Length: {CONTENT_LENGTH}\r\n"
    f"Connection: close\r\n"
    f"\r\n"
)

# Divide body into parts (simulate slow client)
BODY_PARTS = [
    BODY[:10],
    BODY[10:25],
    BODY[25:]
]

def send_in_parts():
    with socket.create_connection((HOST, PORT)) as sock:
        print("[CLIENT] Sending headers...")
        sock.sendall(HEADERS.encode())

        for i, part in enumerate(BODY_PARTS):
            print(f"[CLIENT] Sending part {i+1}...")
            sock.sendall(part.encode())
            time.sleep(1.5)  # Wait between parts

        print("[CLIENT] Waiting for response...")
        response = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk

        print("[CLIENT] Response received:\n")
        print(response.decode())

if __name__ == "__main__":
    send_in_parts()
