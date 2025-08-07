# ApacheBench (ab) Testing for Webserv

## Basic GET Test

```sh
ab -n 1000 -c 100 http://localhost:8080/index.html
```

## Example Test Table

| Test Type          | Command Example                                               |
| ------------------ | ------------------------------------------------------------ |
| 🔁 Longer duration | `ab -n 10000 -c 200 http://localhost:8080/index.html`         |
| 📂 Big files       | `ab -n 1000 -c 50 http://localhost:8080/huge.mp4`            |  
|                    | _Generate file:_ `dd if=/dev/urandom of=huge.mp4 bs=1M count=100` |
| 🔧 CGI load        | `ab -n 1000 -c 50 http://localhost:8080/cgi-bin/test.py`      |
| 🧪 POST test       | `ab -n 500 -c 50 -p post.txt -T text/plain http://localhost:8080/post_body` |
| 🧵 Keep-alive test | `ab -k -n 1000 -c 100 http://localhost:8080/`                |

---

## More Test Ideas

| Target            | Tool / Idea                                              |
| ----------------- | -------------------------------------------------------- |
| 🔁 **Keep-alive** | `ab -k -n 5000 -c 100 http://localhost:8080/`            |
| 💣 **Malformed**  | Custom `curl` or Python clients sending invalid requests |
| 📂 **Big file**   | Serve a 50MB file to test streaming                      |
| 🚧 **Upload**     | Send big POST body using `-p` and `-T`                   |
| 🔁 **Repeated**   | `wrk -t4 -c200 -d30s http://localhost:8080/`             |

---

## Notes

- Make sure your server is running and listening on port 8080.
- Adjust `-n` and `-c` values based on your hardware and what you want to test.
`-n 1000: total number of requests`
`-c 100: number of concurrent clients`
- For POST tests, create a `post.txt` file with your POST body.
- For big file tests, generate a file with:  
  `dd if=/dev/urandom of=huge.mp4 bs=1M count=100`


dd if=/dev/zero of=upload.bin bs=1M count=100
ab -n 10 -c 2 -p upload.bin -T "application/octet-stream" http://localhost:8080/uploads/upload.bin
