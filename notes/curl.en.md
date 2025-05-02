# curl Command and Options

The `curl` command-line tool is used to transfer data to or from a server using supported protocols (HTTP, HTTPS, FTP, etc). Below are some of the most common options used with `curl` for HTTP requests.

## Common Options

### `-X`
**Syntax**: `-X METHOD`

Explicitly specifies the HTTP request method (e.g., GET, POST, DELETE, PUT).

Example:
```bash
curl -X DELETE http://localhost:8080/delete.txt
```

---

### `-d` or `--data`
**Syntax**: `-d "key=value"` or `--data "key=value"`

Sends data in a POST request body. Automatically sets `Content-Type: application/x-www-form-urlencoded`.

Example:
```bash
curl -X POST -d "name=webserv&lang=c++" http://localhost:8080/upload
```

---

### `--data-binary`
**Syntax**: `--data-binary @filename`

Sends the exact binary content of a file in the request body, suitable for images, PDFs, or any non-form data.

Example:
```bash
curl -X POST --data-binary @image.png http://localhost:8080/upload
```

---

## Summary Table

| Option           | Description                              | Typical Use                    |
|------------------|------------------------------------------|----------------------------------|
| `-X`             | Sets the HTTP method                     | DELETE, PUT, POST override      |
| `-d`             | Sends form-encoded data (`key=value`)    | Web form submissions            |
| `--data-binary`  | Sends raw binary file content            | File uploads (images, etc.)     |

---

## Additional Tips
- `curl -v ...` gives verbose output (debugging headers).
- `curl -H "Header: Value" ...` adds custom headers.
- `curl -F "file=@file.png" ...` is used for `multipart/form-data` (file form uploads).

---

For more: `man curl` or [curl.se/docs](https://curl.se/docs)

