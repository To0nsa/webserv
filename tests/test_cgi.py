import unittest
import subprocess
import requests
import time
import os
import signal

SERVER_EXEC = "./bin/webserv"
CONFIG_FILE = "configs/cgi_test.conf"
SERVER_PORT = 8080
SERVER_URL = f"http://localhost:{SERVER_PORT}"

class WebservCGITests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = subprocess.Popen(
            [SERVER_EXEC, CONFIG_FILE],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            preexec_fn=os.setsid
        )
        time.sleep(0.5)

    @classmethod
    def tearDownClass(cls):
        os.killpg(os.getpgid(cls.server.pid), signal.SIGTERM)

    def test_hello_py_get(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/hello.py?foo=bar")
        self.assertEqual(res.status_code, 200)
        self.assertIn("Hello from CGI!", res.text)
        self.assertIn("foo=bar", res.text)

    def test_post_echo(self):
        payload = "name=toonsa&lang=python"
        res = requests.post(f"{SERVER_URL}/cgi-bin/post_echo.py", data=payload, headers={
            "Content-Type": "application/x-www-form-urlencoded"
        })
        self.assertEqual(res.status_code, 200)
        self.assertIn("Input body: name=toonsa", res.text)

    def test_crash(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/crash.py")
        self.assertEqual(res.status_code, 502)

    def test_missing_script(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/doesnotexist.py")
        self.assertEqual(res.status_code, 404)

    def test_non_executable_script(self):
        script_path = "./cgi-bin/hello.py"
        os.chmod(script_path, 0o644)
        res = requests.get(f"{SERVER_URL}/cgi-bin/hello.py")
        self.assertEqual(res.status_code, 403)
        os.chmod(script_path, 0o755)

    def test_timeout(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/sleep.py", timeout=3)
        self.assertEqual(res.status_code, 500)

    def test_env_dump(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/CgiEnv.py")
        self.assertEqual(res.status_code, 200)
        self.assertIn("REQUEST_METHOD = GET", res.text)

    def test_post_cgi(self):
        res = requests.post(f"{SERVER_URL}/cgi-bin/post.py", data="foo=bar&baz=qux")
        self.assertEqual(res.status_code, 200)
        self.assertIn("Hello from POST CGI!", res.text)
        self.assertIn("Method: POST", res.text)
        self.assertIn("foo=bar", res.text)

    def test_no_content_type(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/no_content_type.py")
        self.assertEqual(res.status_code, 200)
        self.assertIn("This has no content-type.", res.text)

    def test_redirect(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/redirect.py", allow_redirects=False)
        self.assertEqual(res.status_code, 302)
        self.assertEqual(res.headers.get('Location'), "/new/location")


    def test_stdout_stderr(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/stdout_stderr.py")
        self.assertEqual(res.status_code, 200)
        self.assertIn("This is stdout", res.text)

    def test_partial_output_crash(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/partial_output.py")
        self.assertEqual(res.status_code, 502)

    def test_hello_sh(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/hello.sh")
        self.assertEqual(res.status_code, 200)
        self.assertIn("Hello from Bash CGI", res.text)

    def test_invalid_headers(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/invalid_headers.py")
        self.assertEqual(res.status_code, 500)

    def test_no_output(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/no_output.py")
        self.assertEqual(res.status_code, 500)

    def test_binary_echo(self):
        res = requests.post(f"{SERVER_URL}/cgi-bin/binary_echo.py", data=b"abc\x00def", headers={'Content-Type': 'application/octet-stream'})
        self.assertEqual(res.status_code, 200)
        self.assertEqual(res.content, b"abc\x00def")

    def test_huge_body(self):
        res = requests.post(f"{SERVER_URL}/cgi-bin/huge_echo_body.py", data="X" * 65536)
        self.assertEqual(res.status_code, 200)
        self.assertIn("Body received (65536 bytes)", res.text)

    def test_headers_only(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/headers_only.py")
        self.assertEqual(res.status_code, 200)

if __name__ == "__main__":
    unittest.main()
