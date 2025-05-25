#!/usr/bin/env python3
import os, sys

sys.stdout.write("Content-Type: text/plain\r\n\r\n")
print("Hello from POST CGI!")
print(f"Method: {os.environ.get('REQUEST_METHOD')}")
print(f"Query: {os.environ.get('QUERY_STRING')}")
print(f"Content-Type: {os.environ.get('CONTENT_TYPE')}")
print(f"Content-Length: {os.environ.get('CONTENT_LENGTH')}")

if os.environ.get("REQUEST_METHOD") == "POST":
    length = os.environ.get("CONTENT_LENGTH")
    if length:
        print("---- Body ----")
        print(sys.stdin.read(int(length)))
