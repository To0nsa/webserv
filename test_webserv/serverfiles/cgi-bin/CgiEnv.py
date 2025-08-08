#!/usr/bin/env python3

import os

print("Content-Type: text/plain\r\n\r\n")
for k, v in os.environ.items():
    print(f"{k} = {v}")

