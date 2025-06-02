import os

BASE = "test/data"
DIRS = [
    "dir",
    "forbidden",
    "secret",
    "upload_store",
    "cgi-bin",
]

def bootstrap():
    print(f"[BOOTSTRAP] Creating test directories in {BASE}")
    os.makedirs(BASE, exist_ok=True)

    for d in DIRS:
        os.makedirs(os.path.join(BASE, d), exist_ok=True)

    # Ensure upload_store is writable (common requirement for tests)
    upload_dir = os.path.join(BASE, "upload_store")
    os.chmod(upload_dir, 0o755)

    # Create a basic index.html at the root
    index_path = os.path.join(BASE, "index.html")
    with open(index_path, "w") as f:
        f.write("<h1>Welcome to Webserv</h1>")

    print("[BOOTSTRAP] ✅ Done.")

if __name__ == "__main__":
    bootstrap()

