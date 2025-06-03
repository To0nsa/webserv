import socket

s = socket.create_connection(("localhost", 8001))
s.sendall(b"GET /cgi-bin/hang.py HTTP/1.1\r\nHost: localhost\r\n\r\n")
s.sendall(b"GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n")

# Read the response from the server
response = s.recv(4096)
print(response.decode())
