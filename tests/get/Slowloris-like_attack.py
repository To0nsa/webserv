import socket
import time

s = socket.socket()
s.connect(('127.0.0.1', 8001))

req = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n"

# Send one character at a time
for c in req:
	s.send(c.encode())
	time.sleep(3)  # 3 seconds between each byte (too slow)

# Finish headers
s.send(b"\r\n\r\n")
