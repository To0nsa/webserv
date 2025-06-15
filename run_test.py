# run_test.py

import subprocess
import sys
import os

def run(cmd, name, failed_tests):
    print(f"\n[RUN] {name}")
    try:
        subprocess.run(cmd, check=True)
        print(f"[PASS] {name} completed successfully")
    except subprocess.CalledProcessError as e:
        print(f"[FAIL] {name} exited with code {e.returncode}")
        failed_tests.append(name)

def main():
    root = os.path.dirname(os.path.abspath(__file__))
    failed_tests = []

    tests = [
        ("test/bootstrap_test_data.py", "bootstrap_test_data.py"),
        ("test/http/test_get.py", "test_get.py"),
        ("test/http/test_http_header_parser.py", "test_http_header_parser.py"),
        ("test/http/test_slow_loris.py", "test_slow_loris.py"),
        ("test/http/test_delete.py", "test_delete.py"),
        ("test/http/test_post.py", "test_post.py"),
        ("test/http/test_redirect.py", "test_redirect.py"),
        ("test/http/test_file_resolution.py", "test_file_resolution.py"),
        ("test/http/test_symlinks_forbidden.py", "test_symlinks_forbidden.py"),
        ("test/http/test_directory_listing.py", "test_directory_listing.py"),
        ("test/http/test_mime_static.py", "test_mime_static.py"),
        ("test/http/stress_test.py", "stress_test.py"),
        ("test/http/stress_cgi.py", "stress_cgi.py"),
        ("test/http/stress_chunked.py", "stress_chunked.py"),
        ("test/http/stress_pipeline.py", "stress_pipeline.py"),
        ("test/http/test_slash_behavior.py", "test_slash_behavior.py"),
        ("test/http/test_connection.py", "test_connection.py"),
        ("test/http/test_cgi.py", "test_cgi.py"),
        # ("test/http/ultra_stress_test.py", "ultra_stress_test.py")
    ]

    for rel_path, name in tests:
        run(["python3", os.path.join(root, rel_path)], name, failed_tests)

    if failed_tests:
        print("\n❌ Some tests failed:")
        for test in failed_tests:
            print(f" - {test}")
        sys.exit(1)
    else:
        print("\n✅ All tests passed!")

if __name__ == "__main__":
    main()
