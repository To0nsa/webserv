import http.client
import time

HOST = "localhost"
PORT = 8080
PATH = "/uploads/test_delete.txt"
BODY = "This file will be deleted in 10 seconds."
CONTENT_TYPE = "text/plain"

def post_file():
    print("[POST] Creating file on server...")

    conn = http.client.HTTPConnection(HOST, PORT)
    headers = {
        "Content-Type": CONTENT_TYPE,
        "Content-Length": str(len(BODY)),
    }

    conn.request("POST", PATH, body=BODY, headers=headers)
    response = conn.getresponse()
    print("[POST] Status:", response.status, response.reason)
    print(response.read().decode())
    conn.close()

def delete_file():
    print("[DELETE] Waiting 10 seconds before deleting...")
    time.sleep(10)

    conn = http.client.HTTPConnection(HOST, PORT)
    conn.request("DELETE", PATH)
    response = conn.getresponse()
    print("[DELETE] Status:", response.status, response.reason)
    print(response.read().decode())
    conn.close()

if __name__ == "__main__":
    post_file()
    delete_file()
