# test/bootstrap_test_data.py

import os

BASE = "test/data"

FILES = {
    "index.html": "<h1>Welcome to Webserv</h1>",
    "style.css": "body { background: #222; color: #eee; }",
    "script.js": "console.log('Hello from JS');",
    "logo.png": b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR"  # Fake PNG header (not valid image)
}

DIRS = {
    "dir": {
        "file.txt": "This is a file inside /dir/"
    },
    "forbidden": {},
    "secret": {},
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
        write_file(path, content if isinstance(content, str) else content, binary=not isinstance(content, str))

    for dir_name, files in DIRS.items():
        dir_path = os.path.join(BASE, dir_name)
        os.makedirs(dir_path, exist_ok=True)
        for fname, fcontent in files.items():
            write_file(os.path.join(dir_path, fname), fcontent)

    for name, html in ERROR_PAGES.items():
        write_file(os.path.join(BASE, name), html)

    print("[BOOTSTRAP] ✅ Done.")


if __name__ == "__main__":
    bootstrap()
