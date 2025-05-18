#!/usr/bin/env python3
import os, sys

sys.stdout.write("Content-Type: text/plain\r\n\r\n")
method = os.environ.get("REQUEST_METHOD")
length = os.environ.get("CONTENT_LENGTH")
body = sys.stdin.read(int(length)) if method == "POST" and length else ""
print(f"Input body: {body}")
