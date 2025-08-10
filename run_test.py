import subprocess
import sys
import os
import shutil

def run(cmd, name, failed_tests):
    print(f"\n[RUN] {name}")
    try:
        subprocess.run(cmd, check=True)
        print(f"[PASS] {name} completed successfully")
    except subprocess.CalledProcessError as e:
        print(f"[FAIL] {name} exited with code {e.returncode}")
        failed_tests.append(name)

def cleanup_upload_store():
    """Delete specific test artifacts and directories after tests,
    but always preserve or re-create required empty.txt files.
    """
    base_dir = os.path.dirname(os.path.abspath(__file__))

    dirs_to_clean = [
        os.path.join(base_dir, "test_webserv", "tester", "data", "upload_store"),
        os.path.join(base_dir, "test_webserv", "tester", "data", "all"),
        os.path.join(base_dir, "test_webserv", "tester", "tester_oskari", "__pycache__"),
    ]

    files_to_delete = [
        os.path.join(base_dir, "test_webserv", "tester", "tester_oskari", "www", "images", "filename.txt"),
        os.path.join(base_dir, "test_webserv", "tester", "tester_oskari", "www", "testfile.txt"),
    ]

    # Empty files that must exist after cleanup
    must_exist_empty = [
        os.path.join(base_dir, "test_webserv", "tester", "data", "dir", "empty.txt"),
        os.path.join(base_dir, "test_webserv", "tester", "data", "upload_store", "empty.txt"),
        os.path.join(base_dir, "test_webserv", "tester", "data", "all", "upload_store", "empty.txt"),
    ]

    KEEP_FILENAMES = {"empty.txt"}
    DEBUG = bool(os.environ.get("CLEANUP_DEBUG"))

    # --- selective file cleanup (no rmtree) ---
    for target_dir in dirs_to_clean:
        if not os.path.exists(target_dir):
            continue
        for root, dirs, files in os.walk(target_dir, topdown=False, followlinks=False):
            # delete files except keepers
            for name in files:
                if name in KEEP_FILENAMES:
                    continue
                fp = os.path.join(root, name)
                try:
                    if DEBUG: print(f"[cleanup] remove file: {fp}")
                    os.remove(fp)
                except FileNotFoundError:
                    pass
                except Exception as e:
                    print(f"⚠️ Could not delete file {fp}: {e}")

            # optional: try to remove now-empty directories (safe)
            # but skipping entirely is safest if tests re-create structure.
            for d in dirs:
                dp = os.path.join(root, d)
                try:
                    os.rmdir(dp)  # succeeds only if empty
                    if DEBUG: print(f"[cleanup] rmdir: {dp}")
                except OSError:
                    # not empty (maybe contains empty.txt) — leave it
                    pass

    # --- explicit deletions, but never touch empty.txt ---
    for file_path in files_to_delete:
        if os.path.basename(file_path) in KEEP_FILENAMES:
            continue
        if os.path.exists(file_path):
            try:
                if DEBUG: print(f"[cleanup] remove explicit: {file_path}")
                os.remove(file_path)
            except Exception as e:
                print(f"⚠️ Could not delete file {file_path}: {e}")

    # --- ensure required empty.txt files exist (recreate if tests removed them) ---
    for fp in must_exist_empty:
        try:
            os.makedirs(os.path.dirname(fp), exist_ok=True)
            with open(fp, "w"):
                pass  # touch => zero bytes
            if DEBUG: print(f"[cleanup] ensure empty.txt: {fp}")
        except Exception as e:
            print(f"⚠️ Could not ensure {fp}: {e}")


def main():
    root = os.path.dirname(os.path.abspath(__file__))
    failed_tests = []

    tests = [
        (["python3", os.path.join(root, "test_webserv/tester/bootstrap_test_data.py")], "bootstrap_test_data.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/test_get.py")], "test_get.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/test_http_header_parser.py")], "test_http_header_parser.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/test_slow_loris.py")], "test_slow_loris.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/test_delete.py")], "test_delete.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/test_post.py")], "test_post.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/test_redirect.py")], "test_redirect.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/test_file_resolution.py")], "test_file_resolution.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/test_symlinks_forbidden.py")], "test_symlinks_forbidden.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/test_directory_listing.py")], "test_directory_listing.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/test_mime_static.py")], "test_mime_static.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/stress_test.py")], "stress_test.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/stress_cgi.py")], "stress_cgi.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/stress_chunked.py")], "stress_chunked.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/stress_pipeline.py")], "stress_pipeline.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/test_slash_behavior.py")], "test_slash_behavior.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/test_connection.py")], "test_connection.py"),
        (["python3", os.path.join(root, "test_webserv/tester/http/test_cgi.py")], "test_cgi.py"),
        #(["python3", os.path.join(root, "test_webserv/tester/http/ultra_stress_test.py")], "ultra_stress_test.py"),
        (["pytest", os.path.join(root, "test_webserv/tester/tester_oskari/test_several.py")], "pytest test_several.py")
    ]

    
    try:
        for cmd, name in tests:
            run(cmd, name, failed_tests)
    finally:
        cleanup_upload_store()

    if failed_tests:
        print("\n❌ Some tests failed:")
        for test in failed_tests:
            print(f" - {test}")
        sys.exit(1)
    else:
        print("\n✅ All tests passed!")

if __name__ == "__main__":
    main()
