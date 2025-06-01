#!/usr/bin/env python3

import os
import signal
import sys

print("Content-Type: text/plain\n")

for i in range(10000):
    print(f"Line {i}")

os.kill(os.getpid(), signal.SIGKILL)  # Terminate without flushing
