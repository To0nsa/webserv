import socket

sock = socket.socket()
sock.connect(("127.0.0.1", 8080))
sock.sendall(b"GET / HTTP/1.1\r\n")
sock.sendall(b"Host: localhost\r\n")
sock.sendall(b"X-Big: " + b"A" * 9000 + b"\r\n\r\n")  # Over 8192 bytes
