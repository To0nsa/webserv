import os
import time
import random
import string
import requests
from concurrent.futures import ThreadPoolExecutor, as_completed

SERVER = os.getenv("WEBSERV_URL", "http://localhost:8080")
ENDPOINT = "/upload_store/stress"
CONCURRENCY = 100
REQUESTS_PER_THREAD = 10
BODY_SIZE = 512  # bytes

def generate_data(size=BODY_SIZE):
    return ''.join(random.choices(string.ascii_letters + string.digits, k=size))

def upload_get_delete(index):
    uri = f"{ENDPOINT}{index}.txt"
    full_url = SERVER + uri
    data = generate_data()

    try:
        # POST
        post = requests.post(full_url, data=data, headers={"Content-Type": "text/plain"}, timeout=3)
        if post.status_code != 201:
            return (index, f"POST {post.status_code}")

        # GET
        get = requests.get(full_url, timeout=2)
        if get.status_code != 200 or get.text.strip() != data.strip():
            return (index, f"GET {get.status_code}/Mismatch")

        # DELETE
        delete = requests.delete(full_url, timeout=2)
        if delete.status_code != 200:
            return (index, f"DEL {delete.status_code}")

        return (index, "OK")
    except Exception as e:
        return (index, f"ERR: {e}")

def stress_test():
    print(f"Starting cleanup stress test with {CONCURRENCY} threads × {REQUESTS_PER_THREAD} requests each...")
    start = time.time()

    with ThreadPoolExecutor(max_workers=CONCURRENCY) as executor:
        futures = []
        for thread_id in range(CONCURRENCY):
            for i in range(REQUESTS_PER_THREAD):
                global_id = thread_id * REQUESTS_PER_THREAD + i
                futures.append(executor.submit(upload_get_delete, global_id))

        success, failure = 0, 0
        for future in as_completed(futures):
            index, result = future.result()
            if result == "OK":
                success += 1
            else:
                failure += 1
                print(f"[{index}] ❌ {result}")

    duration = time.time() - start
    total = CONCURRENCY * REQUESTS_PER_THREAD
    print(f"\n✅ Finished: {success}/{total} succeeded in {duration:.2f}s ({total/duration:.2f} RPS)")
    if failure > 0:
        print(f"❗ {failure} failed requests")

if __name__ == "__main__":
    stress_test()
