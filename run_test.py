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
    """Delete specific test artifacts and directories after tests."""
    base_dir = os.path.dirname(os.path.abspath(__file__))

    # Directories whose contents we want to remove
    dirs_to_clean = [
        os.path.join(base_dir, "test_webserv", "tester", "data", "upload_store"),
        os.path.join(base_dir, "test_webserv", "tester", "data", "all"),
        os.path.join(base_dir, "test_webserv", "tester", "tester_oskari", "__pycache__"),
    ]

    # Specific files to delete
    files_to_delete = [
        os.path.join(base_dir, "test_webserv", "tester", "tester_oskari", "www", "images", "filename.txt"),
        os.path.join(base_dir, "test_webserv", "tester", "tester_oskari", "www", "testfile.txt"),
    ]

    # Clean directory contents
    for target_dir in dirs_to_clean:
        if os.path.exists(target_dir):
            for entry in os.listdir(target_dir):
                path = os.path.join(target_dir, entry)
                try:
                    if os.path.isdir(path):
                        shutil.rmtree(path)
                    else:
                        os.remove(path)
                except Exception as e:
                    print(f"⚠️ Could not delete {path}: {e}")

    # Delete specific files
    for file_path in files_to_delete:
        if os.path.exists(file_path):
            try:
                os.remove(file_path)
            except Exception as e:
                print(f"⚠️ Could not delete file {file_path}: {e}")

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
