import socket

req = (
    # first request: no Connection header, so defaults to keep-alive
    'GET /one HTTP/1.1\r\n'
    'Host: localhost:8080\r\n'
    '\r\n'
    # second request: explicit Connection: close so the server will close after this one
    'GET /two HTTP/1.1\r\n'
    'Host: localhost:8080\r\n'
    'Connection: close\r\n'
    '\r\n'
)

with socket.create_connection(('localhost', 8080)) as s:
    s.sendall(req.encode())
    # Read until the server closes
    data = bytearray()
    while True:
        chunk = s.recv(4096)
        if not chunk:
            break
        data += chunk
print(data.decode(errors='replace'))
