import requests

SERVER_URL = "http://localhost:8080/cgi-bin/CgiEnv.py"

def test_get():
    print("=== TEST: GET CGI ===")
    headers = {
        "User-Agent": "TestClient/1.0",
        "Accept": "*/*",
    }
    response = requests.get(SERVER_URL, headers=headers)
    print("Status:", response.status_code)
    print("Body:\n", response.text)
    assert response.status_code == 200
    assert "REQUEST_METHOD = GET" in response.text
    assert "SCRIPT_NAME = /cgi-bin/CgiEnv.py" in response.text

def test_post():
    print("=== TEST: POST CGI ===")
    headers = {
        "Content-Type": "text/plain",
    }
    data = "test data body"
    response = requests.post(SERVER_URL, headers=headers, data=data)
    print("Status:", response.status_code)
    print("Body:\n", response.text)
    assert response.status_code == 200
    assert "REQUEST_METHOD = POST" in response.text

if __name__ == "__main__":
    test_get()
    print("\n")
    test_post()
