import http.client
import os
import sys
from urllib.parse import urlparse

SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")

def log_request(method, path, status, reason, expected=None):
    if expected is not None and status != expected:
        print(f"❌ {method} {path} → {status} {reason} (expected {expected})")
        sys.exit(1)
    tag = "✅" if method == "DELETE" and status == 200 else "[LOG]"
    print(f"{tag} {method} {path} → {status} {reason}")

def request(method, path, body=None, headers=None, expected=None):
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request(method, path, body=body, headers=headers or {})
    res = conn.getresponse()
    data = res.read().decode(errors="replace")
    conn.close()
    log_request(method, path, res.status, res.reason, expected)
    return res.status, res.reason, data

# ─────────────────────────────────────────────────────────────────────────────
# Helpers
# ─────────────────────────────────────────────────────────────────────────────

def ensure_absent(path):
    status, _, _ = request("GET", path)
    if status == 200:
        request("DELETE", path, expected=200)
    else:
        full = os.path.join(os.getenv("UPLOAD_DIR", "./test/data/upload_store"),
                            os.path.basename(path))
        if os.path.islink(full):
            print(f"[CLEANUP] Removing leftover symlink: {full}")
            os.unlink(full)

def create_file(path, content="x"):
    request("POST", path, body=content,
            headers={"Content-Type": "text/plain"}, expected=201)

# ─────────────────────────────────────────────────────────────────────────────
# Tests
# ─────────────────────────────────────────────────────────────────────────────

def test_file_deletion():
    test_path = "/upload_store/test_delete.txt"
    ensure_absent(test_path)
    create_file(test_path, "hello world")

    status, reason, body = request("DELETE", test_path)
    if status != 200 or "<h1>File test_delete.txt deleted.</h1>" not in body:
        print(f"❌ DELETE {test_path} → {status} {reason}")
        sys.exit(1)

    request("GET", test_path, expected=404)

def test_delete_nonexistent():
    request("DELETE", "/doesnotexist", expected=405)

def test_delete_directory():
    request("DELETE", "/dir/", expected=403)

def test_delete_path_traversal():
    request("DELETE", "/../index.html", expected=403)
    request("DELETE", "/dir/../../secret.txt", expected=403)

def test_delete_with_body():
    test_path = "/upload_store/test_delete_body.txt"
    ensure_absent(test_path)
    create_file(test_path, "data")
    request("DELETE", test_path, body="ignored", expected=200)
    
def test_delete_encoded_filename():
    test_path = "/upload_store/file%20with%20space.txt"
    ensure_absent(test_path)
    create_file(test_path, "content")
    request("DELETE", test_path, expected=200)
    request("GET", test_path, expected=404)
    
def test_delete_file_with_trailing_slash():
    test_path = "/upload_store/test_with_slash.txt"
    ensure_absent(test_path)
    create_file(test_path, "data")
    request("DELETE", test_path + "/", expected=404)
    
def test_delete_case_sensitive():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("delete", "/upload_store/irrelevant.txt")
    res = conn.getresponse()
    data = res.read().decode(errors="replace")
    print(f"[LOG] delete (lowercase) → {res.status} {res.reason}")
    assert res.status == 501, f"Expected 501 Not Implemented, got {res.status}"
    
def test_delete_symlink_to_file():
    """DELETE on a symlink should be forbidden, and the target must remain."""
    test_path   = "/upload_store/symlink_to_file.txt"
    target_path = "/upload_store/real_target.txt"

    # Ensure neither the symlink nor the target exists beforehand
    ensure_absent(test_path)
    ensure_absent(target_path)

    # Create the real target file
    create_file(target_path, "linked")

    # Create a symlink pointing at the real target (if supported)
    try:
        os.symlink(
            os.path.join(os.getenv("UPLOAD_DIR", "./test/data/upload_store"), "real_target.txt"),
            os.path.join(os.getenv("UPLOAD_DIR", "./test/data/upload_store"), "symlink_to_file.txt")
        )

        # Now attempt DELETE on the symlink: should get 403 Forbidden
        request("DELETE", test_path, expected=403)
        
        real_path = os.path.join(os.getenv("UPLOAD_DIR", "./test/data/upload_store"), "real_target.txt")
        if not os.path.isfile(real_path):
            print(f"❌ Expected real target to still exist on disk, but it's missing: {real_path}")
            sys.exit(1)
        print("✅ real_target.txt still exists on disk after DELETE on symlink.")
    except OSError:
        print("[SKIPPED] test_delete_symlink_to_file (symlink not supported)")


def test_delete_root_path_should_be_forbidden():
    """DELETE / should never be allowed (even if mapped)."""
    request("DELETE", "/", expected=403)


def test_delete_deep_nested_file():
    """Ensure nested paths inside upload_store can be deleted."""
    nested_path = "/upload_store/deep/nested/dir/file.txt"
    # Setup directories
    base = os.path.join("test/data/upload_store/deep/nested/dir")
    os.makedirs(base, exist_ok=True)
    with open(os.path.join(base, "file.txt"), "w") as f:
        f.write("deep content")

    # Run test
    request("DELETE", nested_path, expected=200)
    request("GET", nested_path, expected=404)


def test_delete_conflict_dir_vs_file():
    """Ensure DELETE only deletes the file, not a same-named directory."""
    base_path = "/upload_store/conflict"
    file_path = base_path + ".txt"
    dir_path = os.path.join("test/data/upload_store/conflict.txt")
    
    ensure_absent(file_path)
    if not os.path.isdir(dir_path):
        os.makedirs(dir_path, exist_ok=True)

    create_file(file_path, "conflicting file")
    request("DELETE", file_path, expected=200)

    # Directory should still exist
    if not os.path.isdir(dir_path):
        print("❌ conflict directory was deleted (should remain)")
        sys.exit(1)
    else:
        print("✅ conflict directory remained untouched")

# ─────────────────────────────────────────────────────────────────────────────
# Run
# ─────────────────────────────────────────────────────────────────────────────

def run_tests():
    print("[ DELETE Test Suite ]")
    test_file_deletion()
    test_delete_nonexistent()
    test_delete_directory()
    test_delete_path_traversal()
    test_delete_with_body()
    test_delete_encoded_filename()
    test_delete_file_with_trailing_slash()
    test_delete_case_sensitive()
    
    test_delete_symlink_to_file()
    test_delete_root_path_should_be_forbidden()
    test_delete_deep_nested_file()
    test_delete_conflict_dir_vs_file()

if __name__ == "__main__":
    run_tests()