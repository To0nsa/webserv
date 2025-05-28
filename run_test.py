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

    bootstrap = os.path.join(root, "test", "bootstrap_test_data.py")
    test_get = os.path.join(root, "test", "http", "test_get.py")

    run(["python3", bootstrap], "bootstrap_test_data.py")
    run(["python3", test_get], "test_get.py")

    print("\n✅ All tests passed!")

if __name__ == "__main__":
    main()
