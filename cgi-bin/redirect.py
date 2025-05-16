#!/usr/bin/env python3
import sys

sys.stdout.write("Status: 302 Found\r\n")
sys.stdout.write("Location: /new/location\r\n")
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("Redirecting...\n")
