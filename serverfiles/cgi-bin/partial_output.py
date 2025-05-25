#!/usr/bin/env python3
import sys

sys.stdout.write("Content-Type: text/plain\r\n\r\n")
sys.stdout.write("Partial response before crash\n")
sys.stdout.flush()
sys.exit(1)
