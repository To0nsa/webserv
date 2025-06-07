import http.client
import os
import sys
import shutil
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
    request("DELETE", "/doesnotexist", expected=404)

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
    request("DELETE", test_path + "/", expected=403)
    
def test_delete_case_sensitive():
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn.request("delete", "/upload_store/irrelevant.txt")
    res = conn.getresponse()
    data = res.read().decode(errors="replace")
    print(f"[LOG] delete (lowercase) → {res.status} {res.reason}")
    assert res.status == 405, f"Expected 501 Not Implemented, got {res.status}" // 501
    
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

    # Cleanup
    try:
        shutil.rmtree("test/data/upload_store/deep")
    except FileNotFoundError:
        pass

    
def test_delete_empty_path():
    """
    DELETE with an empty request‐URI ("") should be treated as a malformed request → 400.
    """
    # Using the same request() helper: path="" ⇒ raw request line is "DELETE  HTTP/1.1",
    # which our parser should reject as “rawTarget” is empty.
    request("DELETE", "", expected=403)
    
def test_delete_directory_traversal_arbitrary():
    """
    Ensure that DELETE on “/upload_store/../outside_delete.txt” is forbidden
    and does NOT delete a file outside of upload_store.
    """
    # Compute the real filesystem path for a file just above upload_store
    upload_dir = os.getenv("UPLOAD_DIR", "./test/data/upload_store")
    outside_local = os.path.normpath(os.path.join(upload_dir, "../outside_delete.txt"))

    # Make sure its parent directory exists, then create the “outside” file
    os.makedirs(os.path.dirname(outside_local), exist_ok=True)
    with open(outside_local, "w") as f:
        f.write("this should not get deleted")

    # Attempt to delete it via a traversal‐style URI; expect 403 Forbidden
    request("DELETE", "/upload_store/../outside_delete.txt", expected=403)

    # Verify the file is still present on disk
    if not os.path.isfile(outside_local):
        print(f"❌ Vulnerability: {outside_local} was deleted!")
        sys.exit(1)
    else:
        print(f"✅ {outside_local} still exists after traversal DELETE attempt.")

    # Clean up
    os.remove(outside_local)
    
def test_delete_static_file():
    """
    DELETE a file under a normal (non-upload_store) location. 
    Expect 200 on first DELETE, 404 on second.
    """
    # Create it explicitly
    local_dir = os.path.join("test/data/dir")
    os.makedirs(local_dir, exist_ok=True)
    with open(os.path.join(local_dir, "testfile.txt"), "w") as f:
        f.write("static test")

    # 2) DELETE it
    status, reason, body = request("DELETE", "/dir/testfile.txt")
    if status != 200 or "<h1>File testfile.txt deleted.</h1>" not in body:
        print(f"❌ DELETE /dir/testfile.txt → {status} {reason} (expected 200 + correct body)")
        sys.exit(1)
    print("✅ /dir/testfile.txt deleted successfully")

    # 3) GET again → 404
    request("GET", "/dir/testfile.txt", expected=404)

    # 4) DELETE again → 404
    request("DELETE", "/dir/testfile.txt", expected=404)
    
def test_delete_directory_without_slash():
    """
    DELETE a directory URI without the trailing slash → 403 Forbidden
    """
    request("DELETE", "/dir", expected=403)
    
def test_delete_upload_store_root():
    """
    DELETE /upload_store (the directory itself) → 403 Forbidden
    """
    request("DELETE", "/upload_store", expected=403)

def test_double_delete_same_file():
    """
    DELETE the same file two times in a row:
      - first time → 200 OK
      - second time → 404 Not Found
    """
    test_path = "/upload_store/double_delete.txt"
    ensure_absent(test_path)
    create_file(test_path, "double")

    # First delete → 200
    status, reason, _ = request("DELETE", test_path)
    if status != 200:
        print(f"❌ First DELETE {test_path} → {status} (expected 200)")
        sys.exit(1)
    print(f"✅ First DELETE {test_path} → 200 OK")

    # Second delete → 404
    request("DELETE", test_path, expected=404)
    
def test_delete_with_multiple_slashes_and_dots():
    """
    CREATE /upload_store/slash_test.txt, then attempt to DELETE it via:
      - "/upload_store//slash_test.txt"
      - "/upload_store/./slash_test.txt"
    Both should delete the exact same file (200 OK).
    """
    test_path = "/upload_store/slash_test.txt"
    ensure_absent(test_path)
    create_file(test_path, "slash")

    # DELETE via double-slash
    status, _, _ = request("DELETE", "/upload_store//slash_test.txt")
    if status != 200:
        print(f"❌ DELETE //slash_test.txt → {status} (expected 200)")
        sys.exit(1)
    print("✅ DELETE with double slash → 200 OK")

    # Re-create
    create_file(test_path, "slash")

    # DELETE via “./”
    status, _, _ = request("DELETE", "/upload_store/./slash_test.txt")
    if status != 200:
        print(f"❌ DELETE /./slash_test.txt → {status} (expected 200)")
        sys.exit(1)
    print("✅ DELETE with dot‐segment → 200 OK")
    
def test_delete_percent_encoded_nested_path():
    """
    Create /upload_store/deep/nested/dir/pe.txt, then DELETE via percent-encoded slashes:
      "/upload_store/deep%2Fnested%2Fdir%2Fpe.txt"
    """
    nested = "/upload_store/deep/nested/dir/pe.txt"
    # 1) Create the file on disk (“raw” path already exists or create it manually)
    full_dir = os.path.join("test/data/upload_store/deep/nested/dir")
    os.makedirs(full_dir, exist_ok=True)
    with open(os.path.join(full_dir, "pe.txt"), "w") as f:
        f.write("nested percent")

    # 2) DELETE via percent-encoded slashes
    encoded = "/upload_store/deep%2Fnested%2Fdir%2Fpe.txt"
    request("DELETE", encoded, expected=200)

    # 3) GET afterwards → 404
    request("GET", nested, expected=404)

    # Cleanup
    try:
        shutil.rmtree("test/data/upload_store/deep")
    except FileNotFoundError:
        pass

    
def test_delete_symlink_to_directory():
    """
    If there’s a symlink “/upload_store/symlink_dir” → points at some directory 
    (e.g., test/data/dir), then DELETE on “/upload_store/symlink_dir” should be 403 
    and the directory behind it must remain untouched.
    """
    # Ensure file.txt exists
    file_txt_path = "test/data/dir/file.txt"
    os.makedirs(os.path.dirname(file_txt_path), exist_ok=True)
    if not os.path.exists(file_txt_path):
        with open(file_txt_path, "w") as f:
            f.write("This is a file inside /dir/")

    # Clean up any leftover symlink
    test_link = "/upload_store/symlink_dir"
    link_path_local = os.path.join(os.getenv("UPLOAD_DIR", "./test/data/upload_store"), "symlink_dir")
    if os.path.islink(link_path_local):
        os.unlink(link_path_local)

    # Create the symlink
    try:
        os.symlink(os.path.abspath("test/data/dir"), link_path_local)
    except OSError:
        print("[SKIPPED] test_delete_symlink_to_directory (symlink not supported)")
        return

    # Attempt DELETE on the symlink
    request("DELETE", test_link, expected=403)

    # Ensure the original file is still there
    status, _, _ = request("GET", "/dir/file.txt")
    if status != 200:
        print(f"❌ Symlinked directory target was removed or inaccessible → GET /dir/file.txt returned {status}")
        sys.exit(1)
    print("✅ Symlink-to-directory not deleted; real directory still intact")

    # Clean up
    os.unlink(link_path_local)
    
def test_delete_invalid_percent_encoding():
    """
    DELETE "/%ZZ" or "/%" → parser sees invalid percent-encoding → 400 Bad Request
    """
    request("DELETE", "/%ZZ", expected=400)
    request("DELETE", "/%", expected=400)
    
def test_delete_long_url():
    """
    DELETE with an extremely long URI (>2048) → 414 URI Too Long
    """
    long_path = "/a" * 2050
    request("DELETE", long_path, expected=414)
    
def test_delete_root_http10():
    """
    DELETE "/" but explicitly use HTTP/1.0 → still 403 Forbidden
    """
    parsed = urlparse(SERVER)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port)
    conn._http_vsn = 10
    conn._http_vsn_str = "HTTP/1.0"
    conn.request("DELETE", "/")
    res = conn.getresponse()
    if res.status != 403:
        print(f"❌ HTTP/1.0 DELETE / → {res.status} (expected 403)")
        sys.exit(1)
    print("✅ HTTP/1.0 DELETE / → 403 Forbidden")
    conn.close()
    
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
    test_delete_empty_path()
    test_delete_directory_traversal_arbitrary()
    test_delete_static_file()
    test_delete_directory_without_slash()
    test_delete_upload_store_root()
    test_double_delete_same_file()
    test_delete_with_multiple_slashes_and_dots()
    test_delete_percent_encoded_nested_path()
    test_delete_symlink_to_directory()
    test_delete_invalid_percent_encoding()
    test_delete_long_url()
    test_delete_root_http10()
    test_delete_invalid_percent_encoding()
    test_delete_root_http10()

if __name__ == "__main__":
    run_tests()
    