#!/usr/bin/env python3

import os
import sys

# Emit headers (MUST end with blank line)
sys.stdout.write("Content-Type: text/plain\r\n\r\n")

# Read body (for POST)
method = os.environ.get("REQUEST_METHOD")
length = os.environ.get("CONTENT_LENGTH")
body = sys.stdin.read(int(length)) if length and method == "POST" else ""

# Output info
print("Hello from CGI POST handler!")
print(f"Method: {method}")
print(f"Query: {os.environ.get('QUERY_STRING')}")
print(f"Content-Length: {length}")
print(f"Input body: {body}")
