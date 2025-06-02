import os
import random
import string
import time
import requests
from concurrent.futures import ThreadPoolExecutor, as_completed

SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")
ENDPOINT = "/upload_store/stress"
THREADS = 1000
REQUESTS_TOTAL = 10000
BODY_SIZE = int(0.90 * 1024 * 1024)  # ~0.90 MiB
KEEP_ALIVE_HEADERS = {
    "Content-Type": "text/plain",
    "Connection": "keep-alive"
}
session = requests.Session()
posted_paths = []  # to track POSTed files for cleanup

def generate_data():
    return ''.join(random.choices(string.ascii_letters + string.digits, k=BODY_SIZE))

def perform_request(index):
    op = index % 5  # POST, GET, DELETE, INVALID, GET2
    uri = f"{ENDPOINT}{index}.txt"
    url = SERVER + uri
    try:
        if op == 0:
            data = generate_data()
            r = session.post(url, data=data, headers=KEEP_ALIVE_HEADERS, timeout=5)
            if r.status_code == 201:
                posted_paths.append(uri)
            return index, "POST", r.status_code

        elif op == 1 or op == 4:  # GET or GET2
            r = session.get(url, headers={"Connection": "keep-alive"}, timeout=2)
            return index, "GET", r.status_code

        elif op == 2:  # DELETE
            r = session.delete(url, headers={"Connection": "keep-alive"}, timeout=2)
            return index, "DELETE", r.status_code

        elif op == 3:  # INVALID
            s = requests.Session()
            req = requests.Request("BANANA", url, headers={"Connection": "keep-alive"})
            resp = s.send(s.prepare_request(req), timeout=2)
            return index, "INVALID", resp.status_code

    except Exception as e:
        return index, "ERR", f"Exception: {e}"

def cleanup_uploaded_files():
    print("\n[🧹] Cleaning up uploaded files...")
    deleted = 0
    for path in posted_paths:
        try:
            res = session.delete(SERVER + path, timeout=2)
            if res.status_code == 200:
                deleted += 1
        except Exception:
            pass
    print(f"[DONE] {deleted}/{len(posted_paths)} files deleted")

def stress_test():
    print(f"\n[RUN] Stress test: {REQUESTS_TOTAL} requests, {THREADS} threads, 0.90 MiB POSTs\n")
    start = time.time()
    success, fail = 0, 0

    with ThreadPoolExecutor(max_workers=THREADS) as executor:
        futures = [executor.submit(perform_request, i) for i in range(REQUESTS_TOTAL)]

        for future in as_completed(futures):
            index, method, result = future.result()
            if isinstance(result, int) and 200 <= result < 500:
                success += 1
            else:
                fail += 1
                print(f"[{index}] ❌ {method} → {result}")

    duration = time.time() - start
    print(f"\n✅ Done: {success}/{REQUESTS_TOTAL} succeeded in {duration:.2f}s ({REQUESTS_TOTAL/duration:.2f} RPS)")
    if fail:
        print(f"❗ Failures: {fail}")

    cleanup_uploaded_files()

if __name__ == "__main__":
    stress_test()
