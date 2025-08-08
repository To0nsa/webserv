#!/usr/bin/env python3

import os

os.close(1)  # Close stdout
try:
    print("This will fail")  # causes write error
except Exception as e:
    pass  # Suppress local Python error
