#!/usr/bin/env python3
import os, sys

sys.stdout.write("Content-Type: text/plain\r\n\r\n")
print("Hello from CGI!")
print(f"Method: {os.environ.get('REQUEST_METHOD')}")
print(f"Query: {os.environ.get('QUERY_STRING')}")
