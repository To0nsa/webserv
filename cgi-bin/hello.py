#!/usr/bin/env python3

import os
import sys

# Emit proper CGI header
sys.stdout.write("Content-Type: text/plain\r\n\r\n")  # CRLF + CRLF

print("Hello from CGI!")
print(f"Method: {os.environ.get('REQUEST_METHOD')}")
print(f"Query: {os.environ.get('QUERY_STRING')}")
print(f"Content-Length: {os.environ.get('CONTENT_LENGTH')}")
if os.environ.get("REQUEST_METHOD") == "POST":
    print(f"Input body: {sys.stdin.read()}")
else:
    print("Input body: <not read on GET>")
