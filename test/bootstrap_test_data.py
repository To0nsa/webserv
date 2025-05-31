import os
import shutil

BASE = "test/data"
UPLOAD_STORE = os.path.join(BASE, "upload_store")

FILES = {
    "index.html": "<h1>Welcome to Webserv</h1>",
    "style.css": "body { background: #222; color: #eee; }",
    "style.CsS": "body { font-size: 14px; }",
    "script.js": "console.log('Hello from JS');",
    "script.Js": "console.log('Same script with weird casing');",
    "logo.png": b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR",
    "index.html.bak": "<!-- backup copy of index.html -->"
}

DIRS = {
    "dir": {
        "file.txt":     "This is a file inside /dir/",
        "file.unknown": "Binary? Nope—just text to test fallback",
        "testfile.txt": "file for DELETE test"
    },
    "forbidden": {},
    "secret": {},
    "upload_store": {},
}

ERROR_PAGES = {
    "error_403.html": "<h1>403 Forbidden</h1>",
    "error_404.html": "<h1>404 Not Found</h1>"
}


def write_file(path, content, binary=False):
    mode = "wb" if binary else "w"
    with open(path, mode) as f:
        f.write(content)


def bootstrap():
    print(f"[BOOTSTRAP] Creating test files in {BASE}")
    os.makedirs(BASE, exist_ok=True)

    for name, content in FILES.items():
        path = os.path.join(BASE, name)
        binary = not isinstance(content, str)
        write_file(path, content, binary=binary)

    # Create LOGO.PNG to test case insensitivity
    logo_src = os.path.join(BASE, "logo.png")
    logo_dst = os.path.join(BASE, "LOGO.PNG")
    if os.path.exists(logo_src):
        shutil.copyfile(logo_src, logo_dst)

    # Create all subdirectories and files
    for dir_name, files in DIRS.items():
        dir_path = os.path.join(BASE, dir_name)
        os.makedirs(dir_path, exist_ok=True)
        for fname, fcontent in files.items():
            write_file(os.path.join(dir_path, fname), fcontent)

    # Create error pages
    for name, html in ERROR_PAGES.items():
        write_file(os.path.join(BASE, name), html)

    # Ensure upload_store is writable
    os.makedirs(UPLOAD_STORE, exist_ok=True)
    os.chmod(UPLOAD_STORE, 0o755)

    print("[BOOTSTRAP] ✅ Done.")


if __name__ == "__main__":
    bootstrap()
