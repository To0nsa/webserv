#!/usr/bin/env python3
import os, sys

print("Content-Type: text/plain\r\n\r\n")

length = os.environ.get("CONTENT_LENGTH")
if os.environ.get("REQUEST_METHOD") == "POST" and length:
    data = sys.stdin.read(int(length))
    print(f"Body received ({len(data)} bytes)")
else:
    print("No body")
