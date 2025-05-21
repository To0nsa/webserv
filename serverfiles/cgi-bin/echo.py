#!/usr/bin/env python3
import sys

print("Content-Type: text/plain\r\n\r\n")

data = sys.stdin.read()
print(f"Body: {data}")
