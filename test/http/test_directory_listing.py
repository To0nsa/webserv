#!/usr/bin/env python3
import http.client
import os
import sys
import time
import stat
import urllib.parse
from urllib.parse import urlparse

# ─────────────────────────────────────────────────────────────────────────────
# Configuration
# ─────────────────────────────────────────────────────────────────────────────

SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")
parsed = urlparse(SERVER)
HOST   = parsed.hostname
PORT   = parsed.port

DIR_PATH      = "test/data/dir"        # for /dir/
FORBIDDEN_DIR = "test/data/secret"     # for /forbidden/

# ─────────────────────────────────────────────────────────────────────────────
# Helpers
# ─────────────────────────────────────────────────────────────────────────────

def request(path, headers=None):
    conn = http.client.HTTPConnection(HOST, PORT)
    conn.request("GET", path, headers=headers or {})
    response = conn.getresponse()
    body = response.read().decode(errors="replace")
    conn.close()
    return response.status, response.getheader("Location"), body

def assert_redirect(path, expected_location):
    status, location, _ = request(path)
    if status != 301 or location != expected_location:
        print(f"❌ GET {path} → {status}, Location: {location} (expected 301, Location: {expected_location})")
        sys.exit(1)
    print(f"✅ GET {path} → 301 Location: {location}")

def cleanup_test_files():
    """
    Remove any files or directories this test created under:
      - test/data/dir
      - test/data/secret/index.html
    Leave original test/data/dir/file.txt and file.unknown intact.
    """
    # Clean up test/data/dir except original files
    try:
        for fname in os.listdir(DIR_PATH):
            if fname not in ("file.txt", "file.unknown"):
                full = os.path.join(DIR_PATH, fname)
                if os.path.isfile(full) or os.path.islink(full):
                    os.remove(full)
                elif os.path.isdir(full):
                    os.chmod(full, stat.S_IRWXU)
                    for sub in os.listdir(full):
                        subpath = os.path.join(full, sub)
                        if os.path.isfile(subpath) or os.path.islink(subpath):
                            os.remove(subpath)
                        elif os.path.isdir(subpath):
                            os.chmod(subpath, stat.S_IRWXU)
                            os.rmdir(subpath)
                    os.rmdir(full)
    except FileNotFoundError:
        pass

    # Remove test/data/secret/index.html if we created it
    secret_index = os.path.join(FORBIDDEN_DIR, "index.html")
    if os.path.exists(secret_index):
        os.remove(secret_index)

def setup_test_files():
    """
    Ensure that:
      - test/data/dir exists with file.txt and file.unknown
      - test/data/secret/index.html exists with minimal content
    """
    # Prepare /dir/
    os.makedirs(DIR_PATH, exist_ok=True)
    f1 = os.path.join(DIR_PATH, "file.txt")
    if not os.path.exists(f1):
        with open(f1, "w") as f:
            f.write("This is a file inside /dir/")
    f2 = os.path.join(DIR_PATH, "file.unknown")
    if not os.path.exists(f2):
        with open(f2, "w") as f:
            f.write("Binary? Nope—just text to test fallback")
    # Prepare /secret/index.html
    os.makedirs(FORBIDDEN_DIR, exist_ok=True)
    secret_index = os.path.join(FORBIDDEN_DIR, "index.html")
    with open(secret_index, "w") as f:
        f.write("<html><body><h1>Forbidden Index</h1></body></html>")

# ─────────────────────────────────────────────────────────────────────────────
# Tests
# ─────────────────────────────────────────────────────────────────────────────

def test_redirect_trailing_slash():
    """ /dir (no slash) should redirect to /dir/ """
    assert_redirect("/dir", "/dir/")

def test_list_existing_contents():
    """ GET /dir/ → 200, and listing contains file.txt and file.unknown """
    status, _, body = request("/dir/")
    if status != 200:
        print(f"❌ GET /dir/ → {status} (expected 200)")
        sys.exit(1)
    if "file.txt" not in body or "file.unknown" not in body:
        print("❌ /dir/ listing did not include file.txt or file.unknown")
        print("---- Body ----")
        print(body)
        print("--------------")
        sys.exit(1)
    print("✅ /dir/ → 200 OK, contains file.txt and file.unknown")

def test_add_and_list_new_file():
    """ Create newfile.txt, confirm it appears, then delete it. """
    newfile = os.path.join(DIR_PATH, "newfile.txt")
    with open(newfile, "w") as f:
        f.write("Testing autoindex addition")
    time.sleep(0.1)

    status, _, body = request("/dir/")
    if status != 200 or "newfile.txt" not in body:
        print(f"❌ GET /dir/ after adding newfile.txt → {status} or missing newfile.txt")
        print("---- Body ----")
        print(body)
        print("--------------")
        os.remove(newfile)
        sys.exit(1)
    print("✅ /dir/ → 200 OK, contains newfile.txt")

    os.remove(newfile)
    time.sleep(0.1)

    status, _, body = request("/dir/")
    if status != 200 or "newfile.txt" in body:
        print("❌ newfile.txt still present after deletion")
        sys.exit(1)
    print("✅ newfile.txt removed successfully, listing updated")

def test_empty_directory_listing():
    """ Create emptydir/, check appears, then remove it. """
    empty_dir = os.path.join(DIR_PATH, "emptydir")
    os.makedirs(empty_dir, exist_ok=True)
    time.sleep(0.1)

    status, _, body = request("/dir/")
    if status != 200 or "emptydir/" not in body:
        print(f"❌ /dir/ after creating emptydir → {status} or missing emptydir/")
        print("---- Body ----")
        print(body)
        print("--------------")
        os.rmdir(empty_dir)
        sys.exit(1)
    print("✅ /dir/ → 200 OK, contains emptydir/")

    os.rmdir(empty_dir)
    time.sleep(0.1)

    status, _, body = request("/dir/")
    if status != 200 or "emptydir/" in body:
        print("❌ emptydir/ still present after removal")
        sys.exit(1)
    print("✅ emptydir/ removed successfully, listing updated")

def test_forbidden_autoindex_off():
    """
    /forbidden has autoindex off and index.html under test/data/secret/:
      - GET /forbidden → 301 /forbidden/
      - GET /forbidden/ → 200 with index.html
      - GET /forbidden/nonexistent → 404
    """
    assert_redirect("/forbidden", "/forbidden/")

    status, _, body = request("/forbidden/")
    if status != 200 or "<h1>Forbidden Index</h1>" not in body:
        print(f"❌ GET /forbidden/ → {status} (expected 200 serving index.html)")
        sys.exit(1)
    print("✅ /forbidden/ → 200 OK, index.html served")

    status, _, _ = request("/forbidden/nonexistent")
    if status != 404:
        print(f"❌ GET /forbidden/nonexistent → {status} (expected 404)")
        sys.exit(1)
    print("✅ /forbidden/nonexistent → 404 Not Found (nonexistent resource)")

def test_hidden_file_listing():
    """ Create .hidden, confirm appears, then delete it. """
    hidden = os.path.join(DIR_PATH, ".hidden")
    with open(hidden, "w") as f:
        f.write("secret")
    time.sleep(0.1)

    status, _, body = request("/dir/")
    if status != 200 or ".hidden" not in body:
        print(f"❌ /dir/ after creating .hidden → {status} or missing .hidden")
        print("---- Body ----")
        print(body)
        print("--------------")
        os.remove(hidden)
        sys.exit(1)
    print("✅ /dir/ → 200 OK, contains .hidden")

    os.remove(hidden)
    time.sleep(0.1)

    status, _, body = request("/dir/")
    if status != 200 or ".hidden" in body:
        print("❌ .hidden still present after removal")
        sys.exit(1)
    print("✅ .hidden removed successfully, listing updated")

def test_symlink_listing():
    """ Create symlink link.txt→file.txt, confirm appears, then delete. """
    target = os.path.abspath(os.path.join(DIR_PATH, "file.txt"))
    link   = os.path.join(DIR_PATH, "link.txt")

    try:
        if os.path.exists(link):
            os.remove(link)
        os.symlink(target, link)
    except OSError:
        print("[SKIPPED] test_symlink_listing (symlink not supported)")
        return

    time.sleep(0.1)
    status, _, body = request("/dir/")
    if status != 200 or "link.txt" not in body:
        print(f"❌ /dir/ after creating symlink → {status} or missing link.txt")
        print("---- Body ----")
        print(body)
        print("--------------")
        os.remove(link)
        sys.exit(1)
    print("✅ /dir/ → 200 OK, contains link.txt (symlink)")

    os.remove(link)
    time.sleep(0.1)
    status, _, body = request("/dir/")
    if status != 200 or "link.txt" in body:
        print("❌ link.txt still present after removal")
        sys.exit(1)
    print("✅ link.txt removed successfully, listing updated")

def test_unreadable_subdir_listing():
    """
    Create subdir “noread”, strip perms, expect GET /dir/noread/ → 403,
    then restore perms, remove, and verify it disappears.
    """
    subdir = os.path.join(DIR_PATH, "noread")
    os.makedirs(subdir, exist_ok=True)
    os.chmod(subdir, 0)
    time.sleep(0.1)

    status, _, _ = request("/dir/noread/")
    if status != 403:
        print(f"❌ GET /dir/noread/ → {status} (expected 403)")
        os.chmod(subdir, stat.S_IRWXU)
        os.rmdir(subdir)
        sys.exit(1)
    print("✅ /dir/noread/ → 403 Forbidden (unreadable dir)")

    os.chmod(subdir, stat.S_IRWXU)
    os.rmdir(subdir)
    time.sleep(0.1)
    status, _, body = request("/dir/")
    if status != 200 or "noread/" in body:
        print("❌ noread/ still present after removal")
        sys.exit(1)
    print("✅ noread/ removed successfully, listing updated")

def test_trailing_slash_on_file():
    """ GET /dir/file.txt/ should return 404 (file with trailing slash is invalid). """
    status, _, _ = request("/dir/file.txt/")
    if status != 404:
        print(f"❌ GET /dir/file.txt/ → {status} (expected 404)")
        sys.exit(1)
    print("✅ GET /dir/file.txt/ → 404 Not Found")

def test_nested_subdirectory_listing():
    """
    Create subdirectory “sub” with “nested.txt” inside:
      - GET /dir/sub → 301 /dir/sub/
      - GET /dir/sub/ → 200, listing shows nested.txt
    Remove nested.txt and sub/ afterward.
    """
    subdir = os.path.join(DIR_PATH, "sub")
    os.makedirs(subdir, exist_ok=True)
    nested = os.path.join(subdir, "nested.txt")
    with open(nested, "w") as f:
        f.write("inside nested")
    time.sleep(0.1)

    # redirect without slash
    assert_redirect("/dir/sub", "/dir/sub/")

    # listing inside sub/
    status, _, body = request("/dir/sub/")
    if status != 200 or "nested.txt" not in body:
        print(f"❌ /dir/sub/ → {status} or missing nested.txt")
        print("---- Body ----")
        print(body)
        print("--------------")
        os.remove(nested)
        os.rmdir(subdir)
        sys.exit(1)
    print("✅ /dir/sub/ → 200 OK, contains nested.txt")

    os.remove(nested)
    os.rmdir(subdir)
    time.sleep(0.1)
    status, _, body = request("/dir/")
    if status != 200 or "sub/" in body:
        print("❌ sub/ still present after removal")
        sys.exit(1)
    print("✅ sub/ removed successfully, listing updated")

def test_special_chars_filename():
    """
    Create "a & b.txt", ensure appears in /dir/ listing, then delete it.
    """
    name = "a & b.txt"
    filepath = os.path.join(DIR_PATH, name)
    with open(filepath, "w") as f:
        f.write("special")
    time.sleep(0.1)

    status, _, body = request("/dir/")
    if status != 200 or name not in body:
        print(f"❌ /dir/ after creating '{name}' → {status} or missing '{name}'")
        print("---- Body ----")
        print(body)
        print("--------------")
        os.remove(filepath)
        sys.exit(1)
    print(f"✅ /dir/ → 200 OK, contains '{name}'")

    # check percent‐encoded href
    encoded = urllib.parse.quote(name)
    if encoded not in body:
        print(f"❌ href for '{name}' not percent‐encoded as '{encoded}' in listing")
        print("---- Body ----")
        print(body)
        print("--------------")
        os.remove(filepath)
        sys.exit(1)
    print(f"✅ Listing uses percent‐encoded href for '{name}'")

    os.remove(filepath)
    time.sleep(0.1)
    status, _, body = request("/dir/")
    if status != 200 or name in body:
        print(f"❌ '{name}' still present after deletion")
        sys.exit(1)
    print(f"✅ '{name}' removed successfully, listing updated")

def test_parent_directory_link():
    """ Ensure "../" link appears at top of /dir/ listing. """
    status, _, body = request("/dir/")
    if status != 200 or "../" not in body:
        print(f"❌ /dir/ listing missing '../' link")
        print("---- Body ----")
        print(body)
        print("--------------")
        sys.exit(1)
    print("✅ /dir/ → 200 OK, contains '../' link")

def test_sorting_order():
    """
    Create dir "zd" and file "aa.txt". Confirm "zd/" appears before "aa.txt" in /dir/ body.
    """
    dir_zd = os.path.join(DIR_PATH, "zd")
    file_aa = os.path.join(DIR_PATH, "aa.txt")
    os.makedirs(dir_zd, exist_ok=True)
    with open(file_aa, "w") as f:
        f.write("ordering")
    time.sleep(0.1)

    status, _, body = request("/dir/")
    if status != 200:
        print(f"❌ /dir/ → {status} (expected 200)")
        os.remove(file_aa)
        os.rmdir(dir_zd)
        sys.exit(1)
    pos_dir = body.find("zd/")
    pos_file = body.find("aa.txt")
    if pos_dir == -1 or pos_file == -1 or pos_dir > pos_file:
        print("❌ Sorting order incorrect: 'zd/' should come before 'aa.txt'")
        print("---- Body ----")
        print(body)
        print("--------------")
        os.remove(file_aa)
        os.rmdir(dir_zd)
        sys.exit(1)
    print("✅ Sorting: 'zd/' appears before 'aa.txt'")

    os.remove(file_aa)
    os.rmdir(dir_zd)
    time.sleep(0.1)
    status, _, body = request("/dir/")
    if status != 200 or "zd/" in body or "aa.txt" in body:
        print("❌ Cleanup failed: 'zd/' or 'aa.txt' still present")
        sys.exit(1)
    print("✅ 'zd/' and 'aa.txt' removed successfully, listing updated")
    
def test_index_file_precedence():
    """
    Location /with_index/ has both autoindex ON and index index.html.
     - If index.html exists in test/data/with_index, GET /with_index/ → 200 serving index.html
     - Once index.html is removed, GET /with_index/ → 200 with a directory listing that contains file1.txt
    """
    base = "test/data/with_index"
    os.makedirs(base, exist_ok=True)

    # 1) Ensure there’s at least one “regular” file to show in the listing
    file1 = os.path.join(base, "file1.txt")
    if not os.path.exists(file1):
        with open(file1, "w") as f:
            f.write("just a test file")

    # 2) Create index.html
    idx = os.path.join(base, "index.html")
    with open(idx, "w") as f:
        f.write("<html><body><h1>INDEX PRECEDENCE</h1></body></html>")
    time.sleep(0.1)

    # 3) GET /with_index/ → 200, serving index.html
    status, _, body = request("/with_index/")
    if status != 200 or "<h1>INDEX PRECEDENCE</h1>" not in body:
        print(f"❌ GET /with_index/ → {status} (expected 200 with index.html content)")
        os.remove(idx)
        sys.exit(1)
    print("✅ /with_index/ → 200 OK, served index.html (index precedence)")

    # 4) Remove index.html
    os.remove(idx)
    time.sleep(0.1)

    # 5) Now GET /with_index/ → 200 and listing must contain file1.txt
    status, _, body = request("/with_index/")
    if status != 200 or "file1.txt" not in body:
        print(f"❌ GET /with_index/ after removing index.html → {status} or missing file1.txt")
        print("---- Body ----")
        print(body)
        print("--------------")
        # clean up before exit
        if os.path.exists(file1):
            os.remove(file1)
        sys.exit(1)
    print("✅ /with_index/ → 200 OK, directory listing contains file1.txt (autoindex fallback)")

    # 6) Cleanup
    if os.path.exists(file1):
        os.remove(file1)
    time.sleep(0.1)

    # 7) Finally, confirm /with_index/ no longer has file1.txt in the listing
    status, _, body = request("/with_index/")
    if status != 200 or "file1.txt" in body:
        print("❌ file1.txt still present after cleanup")
        sys.exit(1)
    print("✅ file1.txt removed successfully, listing updated")

# ─────────────────────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    cleanup_test_files()
    setup_test_files()

    try:
        test_redirect_trailing_slash()
        test_list_existing_contents()
        test_add_and_list_new_file()
        test_empty_directory_listing()
        test_forbidden_autoindex_off()
        test_hidden_file_listing()
        test_symlink_listing()
        test_unreadable_subdir_listing()
        test_trailing_slash_on_file()
        test_nested_subdirectory_listing()
        test_special_chars_filename()
        test_parent_directory_link()
        test_sorting_order()
        test_index_file_precedence()
    finally:
        cleanup_test_files()
