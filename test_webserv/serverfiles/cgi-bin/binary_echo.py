#!/usr/bin/env python3
import sys, os

sys.stdout.buffer.write(b"Content-Type: application/octet-stream\r\n\r\n")
length = int(os.environ.get("CONTENT_LENGTH", 0))
sys.stdout.buffer.write(sys.stdin.buffer.read(length))
