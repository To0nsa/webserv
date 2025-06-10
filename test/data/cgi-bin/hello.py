#!/usr/bin/env python3
# hello.py - A simple CGI script to print a greeting
import os, sys

sys.stdout.write("Content-Type: text/plain\r\n\r\n")
print("Hello from CGI!")
