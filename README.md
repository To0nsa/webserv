# Webserv

[![Build Status](https://github.com/to0nsa/webserv/actions/workflows/build.yml/badge.svg)](https://github.com/to0nsa/webserv/actions/workflows/build.yml)
[![Docs Status](https://github.com/to0nsa/webserv/actions/workflows/docs.yml/badge.svg?branch=main)](https://to0nsa.github.io/webserv/)
[![License](https://img.shields.io/github/license/to0nsa/webserv.svg)](LICENSE)
[![GitHub Pages](https://img.shields.io/badge/docs-online-blue.svg)](https://to0nsa.github.io/webserv/)
[![clang-tidy](https://img.shields.io/badge/clang--tidy-enabled-brightgreen)](https://clang.llvm.org/extra/clang-tidy/)
[![clang-format](https://img.shields.io/badge/clang--format-automatic-blue)](https://clang.llvm.org/docs/ClangFormat.html)
[![editorconfig](https://img.shields.io/badge/editorconfig-supported-lightgrey)](https://editorconfig.org/)

> A lightweight HTTP/1.1 server written in modern C++20, compliant with the Hive/42 webserv project specifications.

**Webserv** is our first **large-scale C++ project** at Hive/42.
The goal was to implement a lightweight, fully working HTTP/1.1 server from scratch, trying to use modern C++ standard libraries.

The server is designed to be RFC-compliant (following HTTP/1.1 [RFC 7230–7235]) and supports essential features such as:

* Parsing and validating configuration files (Nginx-style syntax).
* Handling GET, POST, DELETE with static files, autoindex, and uploads.
* Executing CGI scripts securely with proper environment and timeouts.
* Multiplexing sockets and CGI pipes in a single poll-based event loop.
* Graceful error handling, timeouts, and connection reuse (keep-alive).

For reference and correctness, Nginx was used as a behavioral benchmark: routing, error responses, and edge cases were compared against it to ensure realistic and compliant behavior.

This project was both a challenge in systems programming and a solid introduction to networking, concurrency, and protocol design in modern C++.
___

## Core of Webserv

| Component          | Responsibility                                                                                                                                                         |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **`Server`**       | Represents a virtual host configuration. Manages binding (host + port), server names, error pages, body size limits, and a collection of `Location` blocks.            |
| **`Location`**     | Encapsulates route-specific configuration. Defines path matching, allowed HTTP methods, root directories, index files, redirects, CGI interpreters, and upload stores. |
| **`runWebserv`**   | Orchestrates the execution of the web server: initializes `Server` objects from the parsed configuration, launches sockets, and enters the event loop.                 |

___

### Program Entrypoint

  1. Parse CLI arguments or fallback to default configuration.
  2. Load and normalize configuration (via config parser).
  3. Validate `Server` and `Location` objects.
  4. Call **`runWebserv()`** to start the server runtime.

The program is designed so that **configuration and validation are complete before runtime begins**, ensuring that only consistent and safe server objects are passed to the execution loop.

___

## Configuration Parsing Flow

This section describes how the configuration parsing logic of **Webserv** works, including the step‑by‑step pipeline and the rules applied during parsing and validation.

<details>
<summary><strong>See Details</strong></summary>

### 1. Tokenization

* **Component:** `Tokenizer`
* **Goal:** Convert raw configuration text into a structured list of tokens.
* **Steps:**

  * Skip UTF‑8 BOM if present.
  * Ignore whitespace, line breaks, and comments (`# ...`).
  * Classify tokens into categories:

    * **Keywords:** `server`, `location`, `listen`, `host`, `root`, `index`, `autoindex`, `methods`, `upload_store`, `return`, `error_page`, `client_max_body_size`, `cgi_extension`.
    * **Identifiers:** Alphanumeric strings with `-`, `.`, `/`, `:` allowed.
    * **Numbers & Units:** Digits with optional single‑letter suffix (`k`, `m`, `g`).
    * **Strings:** Quoted values (single `'` or double `"`).
    * **Symbols:** `{`, `}`, `;`, `,`.
  * Detect and reject invalid characters, control characters, or malformed identifiers.

### 2. Parsing

* **Component:** `ConfigParser`
* **Goal:** Transform token stream into structured objects (`Config`, `Server`, `Location`).
* **Rules:**

  * **Block structure:** Curly braces `{ ... }` delimit `server` and `location` blocks.
  * **Directives:** Each directive must end with `;` unless it opens a block.
  * **Directive placement:** Certain directives are only valid at specific levels:

    * Server level: `listen`, `host`, `server_name`, `error_page`, `client_max_body_size`.
    * Location level: `root`, `index`, `autoindex`, `methods`, `upload_store`, `return`, `cgi_extension`, `cgi_interpreter`.
  * **Nesting:** Locations may not contain other `server` blocks.

### 3. Configuration Objects

* **Server:** Represents a virtual host.

  * Holds host, port, server names, error pages, body size limits, and `Location` blocks.
* **Location:** Defines behavior for a URI path prefix.

  * Includes root directory, index file(s), autoindex flag, allowed methods, redirects, CGI settings, and upload store.

### 4. Normalization

* After parsing, the configuration is **normalized** to ensure consistency and defaults:

  * Missing `client_max_body_size` → default = **1 MB**.
  * Missing `error_page` → add defaults for common errors (403, 404, 500, 502 → `/error.html`).
  * Missing `methods` → defaults to **GET, POST, DELETE**.
  * Locations without `root` → fallback to `/var/www` (unless redirected).
  * Root location (`/`) without `index` → defaults to **index.html**.
* Normalization guarantees that later validation and runtime logic operate on a **complete and uniform** model.

### 5. Validation

* **Component:** `validateConfig`
* **Goal:** Enforce semantic correctness beyond syntax.
* **Checks applied:**

  * **Presence checks:** At least one `location` per `server`.
  * **Path rules:** Location paths must start with `/` and not contain segments beginning with `.`.
  * **Defaults:** Each location must define either a `root` or `return` (but not both with CGI).
  * **Server names:** Must be unique per host\:port, valid per RFC 1035 (no spaces, no control chars, no empty labels).
  * **Ports:** Only one unnamed default server per host\:port pair.
  * **Error pages:** Codes restricted to 400–599.
  * **Redirects:** Only 301, 302, 303, 307, 308 allowed.
  * **Methods:** Only `GET`, `POST`, `DELETE` permitted.
  * **Client body size:** Must be > 0.
  * **CGI:** Extensions must start with a dot, interpreters must map 1‑to‑1 with declared extensions.
  * **Roots & Upload stores:** Must exist and be directories.
  * **Index:** Requires a valid `root`.

### 6. Error Handling

* **Tokenizer:** Throws `TokenizerError` with line/column context when encountering invalid tokens.
* **Parser:** Throws `ConfigParseError` on invalid structure or misplaced directives.
* **Validator:** Throws `ValidationError` with descriptive guidance on fixing invalid configurations.

</details>

___

## Networking `SocketManager`

The heart of Webserv’s I/O: a single `poll()` loop multiplexing **listening sockets**, **client sockets**, and **CGI pipes**, with strict timeouts and robust error recovery.

<details>
<summary><strong>See Details</strong></summary>

* **Listening sockets**: set up `bind()`/`listen()` for each configured host\:port.
* **Event loop**: run non-blocking `poll()` to monitor all descriptors.
* **Connections**:

  * **New connections** → `accept()` → initialize per-client state.
  * **Reads** → receive → parse (supports pipelining) → route.
  * **CGI** → spawn, monitor pipes, enforce timeouts, finalize.
  * **Writes** → stream raw or file-backed responses with keep-alive and backpressure.
* **Timeouts**: enforce idle, header, body, and send deadlines.
* **Errors**: generate accurate HTTP error responses, close cleanly.

</details>

___

## HTTP Handling

This section explains how **Webserv** processes HTTP/1.1 requests end‑to‑end, from bytes on a socket to fully formed responses, and how the server enforces protocol rules, timeouts, and connection reuse.

<details>
<summary><strong>See Details</strong></summary>

### Request Lifecycle (High‑Level)

1. **Accept & Read**
   `SocketManager` accepts client connections on non‑blocking sockets and collects incoming bytes. Per‑connection state tracks **read deadlines** (header/body) and **keep‑alive**.

2. **Parse**
   `HttpRequestParser` incrementally parses:

   * **Start line**: method, request‑target (absolute‑path + optional query), HTTP version (HTTP/1.1).
   * **Headers**: canonicalizes keys; enforces size limits and folding rules; detects `Connection`, `Host`, `Content-Length`, `Transfer-Encoding`, etc.
   * **Body**: supports `Content-Length` and **chunked** transfer decoding. Body size is capped by `client_max_body_size`.

3. **Route**
   `requestRouter` selects a `Server` (host+SNI/server\_name) and the most specific `Location` (longest URI prefix match). It normalizes the filesystem target path and determines whether the request hits **static** content, **autoindex**, **redirect**, **upload**, or **CGI**.

4. **Dispatch**
   Based on method and location rules, it calls `handleGet`, `handlePost`, or `handleDelete`. Unsupported or disallowed → **405** with `Allow` header.

5. **Build Response**
   `responseBuilder` produces status line, headers, and body. It

   * Sets `Content-Type` (MIME by extension), `Content-Length` or `Transfer-Encoding: chunked`, `Connection` (keep‑alive vs close), and error pages.
   * Streams file bodies (sendfile/read+write) with backpressure; can fall back to buffered I/O for CGI and dynamic content.

6. **Send & Reuse**
   `SocketManager` writes the response, respecting **write timeouts** and TCP backpressure. If `Connection: keep-alive` and protocol rules allow, the connection stays open for subsequent pipelined requests.

### Static Files & Autoindex

* **Static files**: Path is resolved from `root` + URI, protecting against traversal. If an **index** is configured and exists for a directory, it is served.
* **Autoindex**: When enabled and no index present, `generateAutoindex` renders a minimal HTML directory listing.
* **ETag/Last‑Modified** *(optional)*: If enabled, responses include validators; otherwise strong caching is avoided. Range requests are not served unless explicitly implemented.

### Errors & Edge Cases

* **400** malformed request, **413** body too large, **414** URI too long, **404/403** missing or forbidden paths, **405** method not allowed.
* **408/504** on header/body/send timeouts. **431** for oversized header sections.
* **5xx** on internal faults, filesystem errors, or CGI failures (see below).

</details>

___

## Request & CGI Handling

This section details how POST uploads, multipart forms, and CGI programs are handled, including sandboxing and timeout policy.

<details>
<summary><strong>See Details</strong></summary>

### POST Uploads & Multipart

* **Content dispatch**: `handlePost` inspects `Content-Type` and forwards to specialized handlers.
* **application/x-www-form-urlencoded**: Parsed into key/value pairs. Small payloads are buffered; oversized inputs fail fast with **413**.
* **multipart/form-data**: `handleMultipartForm` parses parts lazily to disk, honoring per‑file and aggregate size limits. Saved files go to the `upload_store` defined on the matched `Location`.
* **application/octet-stream / arbitrary media**: Stored as a single file in `upload_store` with a server‑generated filename when no name is provided.
* **Overwrite policy**: Configurable (e.g., reject on conflict or rename). Errors yield **409** (conflict) or **500** depending on the cause.

### CGI Execution Model

* **When CGI triggers**: A request is routed to CGI when the target path matches a configured `cgi_extension` (e.g., `.py`, `.php`) and an interpreter is set, or when the `Location` forces CGI.

* **Environment**: `handleCgi` constructs a POSIX environment per CGI/1.1:

  * `REQUEST_METHOD`, `QUERY_STRING`, `CONTENT_LENGTH`, `CONTENT_TYPE`, `SCRIPT_FILENAME`, `PATH_INFO`, `SERVER_PROTOCOL`, `SERVER_NAME`, `SERVER_PORT`, `REMOTE_ADDR`, and `HTTP_*` for forwarded headers.
  * Working directory is the script directory; stdin is the request body (streamed or buffered based on size).

* **Process lifecycle**:

  1. Create pipes for **stdin**/**stdout**, fork, exec interpreter + script.
  2. Parent polls child pipes non‑blocking with **CPU/IO activity watchdogs**.
  3. Enforces **hard timeouts** (startup, read, total runtime). On violation → terminate child.

* **Output parsing**: CGI writes `Status: 200 OK\r\n`, arbitrary headers, blank line, then body. The server:

  * Parses CGI headers (maps/filters hop‑by‑hop), merges with server headers.
  * If `Location` header without body → treat as **redirect** per CGI spec.
  * Otherwise body is streamed back to the client.

* **Failure mapping**:

  * Exec/spawn error → **502 Bad Gateway**.
  * Timeout or premature exit → **504 Gateway Timeout**.
  * Malformed CGI headers → **502**.
  * Script wrote nothing (unexpected EOF) → **502**.

* **Security & Limits**:

  * Drop privileges/chroot *(if configured)*; never inherit ambient FDs; sanitize environment.
  * Enforce **max body size**, **max headers**, **max response size** (protects RAM), and per‑request **open‑file caps**.

### GET/DELETE Semantics

* **GET**: Serves static files, autoindex pages, or dispatches to CGI. Conditional GETs (If‑Modified‑Since/If‑None‑Match) may be supported depending on build settings.
* **DELETE**: Removes targeted file from the resolved root when allowed in `methods`. On success → **204 No Content**; on missing/forbidden → **404/403**.

### Response Builder (Recap)

* Centralizes status line + headers, error page selection, and body streaming. Ensures `Content-Length` vs `chunked` consistency and keeps **connection semantics** correct across errors and CGI boundaries.

</details>

___

## Flow Overview - End‑to‑End Runtime

This is the complete lifecycle from configuration to bytes on the wire, aligned with the current codebase.

<details>
<summary><strong>See Details</strong></summary>

1. **Startup & Configuration**

* **Tokenizer → ConfigParser → normalizeConfig → validateConfig**

  * Tokenize config, build `Server`/`Location` graphs, apply defaults (client body size, methods, roots, index, error pages), and enforce semantic rules (paths, redirects, methods, CGI mapping).
* **Bootstrap**

  * Instantiate `Server` objects, bind/listen on configured host\:port pairs, pre‑compute route tables and error pages.

2. **Event Loop (SocketManager)**

* Single non‑blocking `poll()` loop over listening sockets, client sockets, and CGI pipes.
* Per‑connection state tracks read/write buffers, deadlines (header/body/send), and keep‑alive.
* **Accept** new connections ➜ initialize state.

3. **Read → Parse (HttpRequestParser)**

* Accumulate bytes until `"\r\n\r\n"` (header terminator) is found.
* **Start line**: validate method token, request‑target, version.
* **Headers**: normalize keys, reject duplicates where disallowed, check `Content‑Length`/`Transfer‑Encoding` (conflict, format), enforce `Host` on HTTP/1.1, cap header section size.
* **URL/Host routing hint**: derive effective `Url` and matched server affinity; store `Host`, `Query`, `Content‑Length`.
* **Body**:

  * If `Transfer‑Encoding: chunked` ➜ incremental chunk decoding; forbid trailers; enforce `client_max_body_size`.
  * Else if `Content‑Length` ➜ wait until full body; enforce size cap; detect pipelined next request beyond the declared length.
  * GET/DELETE: treat any extra bytes as pipeline, not body.

4. **Routing (requestRouter)**

* **Directory‑slash redirect** when target resolves to a directory but URI lacks trailing `/`.
* **Location selection**: exact match, else longest prefix.
* **Configured redirect (`return` 301/302/307/308)** short‑circuit.
* **Method gate**:

  * 501 if method not implemented (only GET/POST/DELETE supported).
  * 405 if not allowed by `Location`’s `methods`.

5. **Dispatch (methodsHandler)**

* **GET**

  * Resolve physical path under `root` (no traversal, no symlinks).
  * If directory:

    * If index exists ➜ serve file.
    * Else if `autoindex on` ➜ generate HTML listing.
    * Else ➜ 403.
  * If regular file ➜ serve with MIME type detection. Small files buffered, large files streamed.
* **POST**

  * Preconditions: non‑empty body, size ≤ `client_max_body_size`, `upload_store` configured.
  * Determine safe target path under `upload_store` (percent‑decode, canonicalize, reject symlinks, mkdir ‑p).
  * Content‑type switch:

    * `multipart/form-data` ➜ stream first file part to disk (boundary parsing, per‑part size cap).
    * `application/x‑www‑form‑urlencoded` ➜ parse kv pairs; persist rendered HTML summary.
    * Other types ➜ raw body saved as a file.
  * 201 on success with minimal HTML confirmation.
* **DELETE**

  * Resolve path; reject directories/symlinks; remove regular file; reply 200 with HTML confirmation.

6. **CGI (handleCgi) - when location/extension triggers**

* **Spawn**

  * Write request body to temp file; create output temp file.
  * Build `execve` argv (interpreter + script) and CGI/1.1 env (`REQUEST_METHOD`, `QUERY_STRING`, `SCRIPT_FILENAME`, `PATH_INFO`, `SERVER_*`, `HTTP_*`, etc.).
  * `fork()` child ➜ `dup2(stdin/out)` to temp fds ➜ `chdir(script dir)` ➜ `execve()`.
* **Supervision**

  * Parent polls pipes/Fds with timeouts; on inactivity/overrun ➜ kill and 504/502.
* **Finalize**

  * Parse output file head for CGI headers (`Status:`, `Content‑Type:`) until `CRLF CRLF`.
  * Compute body offset and size, then return a **file‑backed** response pointing at CGI output (no copy), with correct status and content type.
  * Ensure temp files are unlinked/cleaned after send.

7. **Response Building (responseBuilder/HttpResponse)**

* Build status line + headers; choose reason phrase; select custom error page if configured.
* Set `Content‑Type`, `Content‑Length` (or stream file length) and connection semantics.
* **Keep‑alive policy**

  * HTTP/1.1: keep‑alive by default unless `Connection: close` **or** fatal status (e.g., 400/408/413/500) forces close.
  * HTTP/1.0: close by default unless `Connection: keep‑alive`.
* For redirects: set `Location`; body often omitted/minimal.

8. **Write → Reuse/Close**

* Non‑blocking writes honor backpressure and send timeouts.
* If `keep‑alive` and no close‑forcing status ➜ retain connection for next pipelined request (parser resumes at leftover bytes).
* Else ➜ close socket and release all per‑connection resources.

9. **Error Mapping & Hardening**

* Parser/Router/FS/CGI errors mapped to precise HTTP codes (400/403/404/405/408/411/413/414/415/431/500/501/502/504/505).
* Safeguards: normalized paths, no `..`, symlink denial, header/body caps, per‑request timeouts, upload store confinement, and strict header validation.

</details>

___

## Continuous Integration & Documentation

This project leverages **GitHub Actions** to ensure code quality, stability, and up-to-date documentation.

<details>
<summary><strong>See Details</strong></summary>

### CI Pipeline

* Runs automatically on pushes and pull requests to `main` and `dev`.
* Includes manual triggers (`workflow_dispatch`) and dependency checks after successful builds.

**Jobs Overview:**

| Job          | Description                                                                                      |
| ------------ | ------------------------------------------------------------------------------------------------ |
| 🔨 **Build** | Compiles the project using the provided `Makefile` to ensure successful builds.                  |
| 🧪 **Test**  | Builds the server, runs Python test suite against a live instance, and captures logs on failure. |
| 📚 **Docs**  | Generates Doxygen documentation (with Graphviz diagrams) and deploys it to **GitHub Pages**.     |

</details>

Every code change is built, tested, and documented automatically, ensuring a robust development workflow and always-available reference docs.

___

## Documentation

This section describes how project documentation is generated, structured, and published.

<details>
<summary><strong>See Details</strong></summary>

### 1. Doxygen-Powered

* Documentation is generated automatically from **source code comments** and **Markdown files**.
* `README.md` serves as the **entry point**, offering an overview and links to modules.

### 2. Graphical Support

* **Graphviz** integration produces:

  * **Class diagrams** to illustrate object hierarchies.
  * **Call graphs** to visualize execution flow.
  * **Dependency graphs** to map relationships between modules.
* These visuals improve comprehension of the server’s architecture.

### 3. Navigation & Browsing

* The source browser cross-references **functions, classes, and files**.
* Each documented entity links directly to its definition in the codebase.
* Groups (`@defgroup`, `@ingroup`) provide thematic navigation across modules (e.g., `config`, `core`, `http`).

### 4. Deployment

* Documentation is built in **CI/CD pipelines**.
* Published automatically via **GitHub Pages** from the `docs/html` directory.
* Ensures the latest version is always available for contributors and maintainers.

### 5. Best Practices

* Consistent **Doxygen-style headers** across `.hpp` and `.cpp` files.
* Markdown files complement code documentation with **high-level design notes** and **workflow explanations**.
* Together, these guarantee both **low-level API reference** and **high-level architectural guidance**.

</details>

___

## Project Structure Overview

```bash
webserv
├── 📁 .github/               # GitHub Actions CI workflows and PR/issue templates
│   └── workflows/
│       ├── ci.yml             # CI workflow: builds with Makefile
│       └── docs.yml           # Doxygen documentation generation & GitHub Pages deploy
├── 📁 include/                # All public project headers, grouped by module (config, http, core, etc.)
├── 📁 src/                    # Source files, mirrors the include/ structure
├── 📁 test_webserv/           # Unit tests
├── 📁 configs/                # Default config file
├── 📁 docs/                   # Documentation generated by doxygen
├── .clang-format               # Enforces formatting rules (4-space indent, K&R braces, etc.)
├── .editorconfig               # Shared IDE/editor config for consistent style
├── .gitattributes              # Defines merge/diff rules for Git (e.g. binary files)
├── .gitignore                  # Files and folders ignored by Git (e.g. build/, *.o)
├── ACTIONPLAN.md               # Project-level planning/roadmap
├── DOXYGENSTYLEGUIDE.md        # Doxygen conventions for documenting code
├── Doxyfile                    # Main config for Doxygen documentation generation
├── LICENSE                     # Project license
├── Makefile                    # Build system entry point
├── README.md                   # Main README
├── STYLEGUIDE.md               # Coding conventions for naming, layout, formatting
├── run_test.py                 # Entrypoint for python tests
├── webserv.subject.pdf         # Original subject specification for the project
```

___

## Build & Test Instructions

### Build with Makefile

```bash
make
./bin/webserv <path-to-config.conf>
```

> The default goal is `all`. The binary is produced at `bin/webserv`.

### Available Makefile Targets

| Command                  | Description                                                                                                                     |
| ------------------------ | ------------------------------------------------------------------------------------------------------------------------------- |
| `make`                   | Build the project in release mode (C++20, `-O3 -flto -DNDEBUG -march=native`).                                                  |
| `make re`                | Clean everything and rebuild from scratch.                                                                                      |
| `make clean`             | Remove object files and dependency files in `objs/`.                                                                            |
| `make fclean`            | Remove the executable, `bin/`, and all build artifacts (also runs `clean`).                                                     |
| `make install_test_deps` | Create a local Python venv in `.venv/` and install `requirements-test.txt`.                                                     |
| `make test`              | Build, start the server in background with `./test_webserv/tester/config/tester.conf`, run `run_test.py`, then stop the server. |
| `make format`            | Run `clang-format -i` on all listed sources and headers.                                                                        |
| `make help`              | Print a categorized list of available targets.                                                                                  |

### Notes

* Objects and auto-generated deps are stored under `objs/` (built via `-MMD -MP`).
* The build uses explicit source lists (no wildcards) for deterministic builds.
* The test rule writes the PID to `.webserv_test.pid` and cleans it up on success/failure.
* Ensure `python3-venv` and `clang-format` are installed on your system.

___

## License

This project is licensed under the terms of the [MIT License](LICENSE).

___
