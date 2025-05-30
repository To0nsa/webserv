# run_test.py

import subprocess
import sys
import os

def run(cmd, name):
    print(f"\n[RUN] {name}")
    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError as e:
        print(f"[FAIL] {name} exited with code {e.returncode}")
        sys.exit(e.returncode)
    print(f"[PASS] {name} completed successfully")

def main():
    root = os.path.dirname(os.path.abspath(__file__))

    bootstrap           = os.path.join(root, "test", "bootstrap_test_data.py")
    test_get            = os.path.join(root, "test", "http", "test_get.py")
    test_raw_malformed  = os.path.join(root, "test", "http", "test_http_header_parser.py")
    test_slow_lors      = os.path.join(root, "test", "http", "test_slow_loris.py")
    test_delete        = os.path.join(root, "test", "http", "test_delete.py")
    test_file_resolution = os.path.join(root, "test", "http", "test_file_resolution.py")

    run(["python3", bootstrap], "bootstrap_test_data.py")
    run(["python3", test_raw_malformed], "test_http_header_parser.py")
    run(["python3", test_file_resolution], "test_file_resolution.py")
    run(["python3", test_get], "test_get.py")
    run(["python3", test_delete], "test_delete.py")
    run(["python3", test_slow_lors], "test_slow_loris.py")

    print("\n✅ All tests passed!")

if __name__ == "__main__":
    main()
