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
        # Start the server
        cls.server = subprocess.Popen(
            [SERVER_EXEC, CONFIG_FILE],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            preexec_fn=os.setsid  # to kill entire group later
        )
        time.sleep(0.5)  # wait for server to boot

    @classmethod
    def tearDownClass(cls):
        # Kill the entire process group
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
        os.chmod(script_path, 0o644)  # remove execute
        res = requests.get(f"{SERVER_URL}/cgi-bin/hello.py")
        self.assertEqual(res.status_code, 403)
        os.chmod(script_path, 0o755)  # restore

    def test_timeout(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/sleep.py", timeout=3)
        self.assertEqual(res.status_code, 500)

    def test_env_dump(self):
        res = requests.get(f"{SERVER_URL}/cgi-bin/CgiEnv.py")
        self.assertEqual(res.status_code, 200)
        self.assertIn("REQUEST_METHOD = GET", res.text)

if __name__ == "__main__":
    unittest.main()
