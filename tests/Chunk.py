import http.client
from time import sleep

conn = http.client.HTTPConnection("127.0.0.1", 8080)
conn.putrequest("POST", "/uploads")
conn.putheader("Transfer-Encoding", "chunked")
conn.putheader("Content-Type", "text/plain")
conn.endheaders()

def send_chunk(data):
    chunk = f"{len(data):X}\r\n{data}\r\n"
    conn.send(chunk.encode())

send_chunk("Hello")
sleep(2)  # Simulate delay
send_chunk(" ")
sleep(2)  # Simulate delay
send_chunk("World")
conn.send(b"0\r\n\r\n")  # End of chunks

res = conn.getresponse()
print(res.status, res.reason)
print(res.read().decode())
