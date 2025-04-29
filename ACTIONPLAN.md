# Webserv Project - Action Plan

___

## SPRINT 1 - minimal non-blocking HTTP server

<details>
<summary><strong>See Sprint 1</strong></summary>

## Sprint Goal

Build a **minimal non-blocking HTTP server** that:

- Accepts incoming client connections (non-blocking).
- Parses a basic HTTP request line (GET / HTTP/1.1).
- Sends a **hardcoded 200 OK** "Hello World" HTTP response.
- Uses a **single `poll()` loop** for all socket operations.
- Compiles cleanly using the **Makefile**.

___

## Developer Assignments

### Dev 1 - Networking

- Create `SocketManager`:
  - Bind a TCP socket to port 8080.
  - Set socket to non-blocking mode (O_NONBLOCK).
- Create `PollManager`:
  - Handle the `poll()` loop for all FDs.
  - Accept new clients when the server socket has a POLLIN event.
  - Set new client sockets to non-blocking.

**Output**: Accept clients without blocking.

### Dev 2 - HTTP Parsing + Response

- Create `HttpRequestParser`:
  - Parse the first request line (method, path, HTTP version).
- Create `HttpResponseBuilder`:
  - Always send a hardcoded 200 OK response:

```bash
HTTP/1.1 200 OK
Content-Length: 13

Hello, world!
```

**Output**: Clients receive "Hello World" on any request.

### Dev 3 - Configuration Management

- Create `Config` class:
  - Hardcode default settings (port 8080).
  - No real `.conf` file parsing yet.
- Validate that the **Makefile** builds correctly.
- Create a minimal `main.cpp`:
  - Instantiate `SocketManager`, `PollManager`.
  - Start server loop.

**Output**: Server starts and runs correctly.

___

### Tasks Summary

| Task | Owner | Priority | Size |
|:-----|:------|:---------|:-----|
| Setup minimal SocketManager | Dev 1 | P0 | M |
| Setup PollManager and poll() loop | Dev 1 | P0 | M |
| Accept new client connections non-blocking | Dev 1 | P0 | M |
| Parse minimal HTTP request line | Dev 2 | P0 | S |
| Send minimal hardcoded HTTP 200 OK response | Dev 2 | P0 | S |
| Setup minimal Config class | Dev 3 | P0 | S |
| Create minimal `main.cpp` to wire modules | Shared | P0 | XS |
| Validate and use the updated Makefile | Dev 3 | P0 | Done |

___

### Success Criteria

- `make` and `make debug` succeed with no warnings.
- `./bin/webserv` starts cleanly and binds to port 8080.
- Browser or telnet to `localhost:8080` returns "Hello World".
- Non-blocking `poll()` loop handles all activity.
- Clean, modular code respecting project structure.

___

## Priorities

| Priority | Description |
|:---------|:------------|
| 🔴 P0 | Networking (poll, non-blocking accept) |
| 🔴 P0 | HTTP parsing and 200 OK response |
| 🔴 P0 | Config skeleton and clean project start |

___

## Final Deliverable

> A **working non-blocking HTTP server** that accepts multiple connections and replies "Hello World".

___

## Mindset Reminder

> Focus ONLY on Sprint 1 scope.
> No CGI, no POST, no DELETE, no sessions yet.

First get a **rock-solid event loop** with basic GET handling.

</details>

___

## SPRINT 2 - Configuration & Dynamic Serving

<details>
<summary><strong>See Sprint 2</strong></summary>

## Sprint 2 Goal

Extend the server to:

- Parse real `.conf` configuration files.
- Support multiple servers and ports.
- Serve static files based on routing rules.
- Display error pages when appropriate.

Make the server dynamic and compliant with HTTP hosting needs.

___

## Developer Assignments (sprint 2)

### Dev 3 - Configuration Parser

- Parse `.conf` files inspired by Nginx syntax.
- Support multiple `server` blocks with:
  - Listen ports.
  - Server names.
  - Roots.
  - Error pages.
  - Upload paths.
  - Accepted methods.
- Map configuration into in-memory structures.

**Output**: Server config fully loaded before boot.

### Dev 2 - Extended HTTP Parsing and Responses

- Extend `HttpRequestParser` to parse full headers.
- Improve `HttpResponseBuilder`:
  - Serve real files from configured `root`.
  - Send 404 error page if file not found.
- Start handling simple `POST` (accepting upload body).

**Output**: Server responds with real file contents or proper errors.

### Dev 1 - PollManager and Routing Enhancements

- Manage multiple server sockets if different ports are needed.
- Extend `PollManager`:
  - Accept connections on multiple FDs.
  - Match incoming connections to the correct server config using Host header.

**Output**: Correct server config selected per request.

___

### Tasks Summary (sprint 2)

| Task | Owner | Priority | Size |
|:-----|:------|:---------|:-----|
| Parse `.conf` into Config objects | Dev 3 | P0 | L |
| Support multiple `server` blocks | Dev 3 | P0 | M |
| Parse full HTTP headers | Dev 2 | P0 | M |
| Serve static files from root | Dev 2 | P0 | M |
| Serve 404 error page when needed | Dev 2 | P0 | S |
| Bind and listen on multiple ports | Dev 1 | P0 | M |
| Match requests to servers based on Host header | Dev 1 | P0 | M |
| Improve PollManager to handle multiple listening FDs | Dev 1 | P0 | M |

___

## Sprint 2 Success Criteria

- The server can load and parse a real `.conf` file.
- The server can serve static files correctly based on route.
- Returns 404 error page if file not found.
- Handles multiple listening ports simultaneously.
- Chooses the right server block based on the Host header.

___

## Key Deliverables

- Full configuration parsing.
- Virtual hosting support.
- Real static file serving.
- Default and custom error pages.
- Multiple ports listening.

___

## Important Notes

- Post/Upload handling will be basic initially, improved in later sprints.
- Stress tests with multiple concurrent clients can start after Sprint 2.
- Ensure server resilience against invalid/malformed requests.

___

## Mindset Reminder (sprint 2)

> Configuration-driven design is the key for Sprint 2.
>
> No hardcoded routes anymore: everything must come from the `.conf` file.

Prepare the foundation for CGI, Uploads, Sessions, and Bonus work later!

</details>

___

## SPRINT 3 - Full HTTP/1.1 Compliance

<details>
<summary><strong>See Sprint 3</strong></summary>

### Sprint Goal (sprint 3)

Finalize HTTP/1.1 compliance:

- Support full GET, POST, DELETE handling.
- Implement file upload (POST).
- Execute CGI scripts for dynamic responses.
- Enforce error pages and body size limits.

Sprint 3 completes all mandatory requirements of the subject.

___

### Developer Assignments (sprint 3)

#### Dev 2 - HTTP Methods

- Implement full HTTP method support:
  - `GET`: Serve static files (already started).
  - `POST`: Accept file uploads, save to correct directory.
  - `DELETE`: Remove files when authorized.
- Check allowed methods per route and return 405 if method not allowed.

**Output**: Server processes all major HTTP methods correctly.

#### Dev 1 - CGI Integration

- Implement CGI execution:
  - Fork for CGI requests only.
  - Setup environment variables (PATH_INFO, QUERY_STRING, etc.).
  - Pass request body to CGI via stdin.
  - Read CGI output and forward as HTTP response.

**Output**: Server supports dynamic scripting (PHP, Python, etc.).

#### Dev 3 - Advanced Configuration and Error Management

- Extend configuration parsing:
  - Define allowed methods, upload directories, CGI handlers per route.
- Implement error handling:
  - Serve 400 (Bad Request), 403 (Forbidden), 404 (Not Found), 413 (Payload Too Large), 500 (Server Error) pages.

**Output**: Server configurable for routes, methods, uploads, error pages.

___

### Tasks Summary (sprint 3)

| Task | Owner | Priority | Size |
|:-----|:------|:---------|:-----|
| Implement POST upload handling | Dev 2 | P0 | M |
| Implement DELETE method handling | Dev 2 | P0 | M |
| Implement method checking per route | Dev 2 | P0 | S |
| Implement CGI launcher and environment | Dev 1 | P0 | L |
| Setup CGI stdin/stdout handling | Dev 1 | P0 | M |
| Parse upload and method config | Dev 3 | P0 | M |
| Serve custom error pages (400/403/404/413/500) | Dev 3 | P0 | M |

___

### Sprint 3 Success Criteria

- Server fully supports GET, POST, DELETE.
- File uploads are stored in configured directories.
- CGI scripts run correctly and output valid HTTP responses.
- Errors return correct status codes and pages.
- Body size limits are enforced.

___

### Critical Compliance Targets

| Subject Requirement | Status After Sprint 3 |
|:--------------------|:----------------------|
| Full method support | ✅ Done |
| File uploads via POST | ✅ Done |
| CGI execution | ✅ Done |
| Body size limit respected | ✅ Done |
| Error page handling | ✅ Done |

___

### Mindset Reminder (sprint 3)

> This sprint finishes all **mandatory** work.
>
> Bonuses (cookies, sessions, WebSocket) **only** happen after Sprint 3 total success.

Ensure that everything works **cleanly, reliably, and resiliently**.

</details>

___

## SPRINT 4 - Cookies and Sessions

<details>
<summary><strong>See Sprint 4</strong></summary>

> Bonus sprint: Implement **stateful client sessions** via **HTTP Cookies**.

This feature increases server functionality for real-world usage and adds significant value to the defense bonus points.

___

### Developer Assignments (sprint 4)

#### Dev 2 - Cookie Parsing and Handling

- Parse `Cookie:` header from incoming requests.
- Extract session ID if present.
- Handle missing/invalid cookies cleanly (assign new session).

#### Dev 3 - Session Management

- Implement an in-memory session store (map of session IDs -> session data).
- Create and track sessions with expiration or timeout policy.
- Provide methods to create, retrieve, and delete sessions.

#### Dev 1 - Response Cookies

- Extend `HttpResponseBuilder` to:
  - Add `Set-Cookie:` headers when a new session is created.
  - Optionally refresh cookie expiration for active sessions.

___

### Tasks Summary (sprint 4)

| Task | Owner | Priority | Size |
|:-----|:------|:---------|:-----|
| Parse incoming `Cookie:` headers | Dev 2 | P2 | M |
| Store/retrieve session data server-side | Dev 3 | P2 | M |
| Generate secure session IDs | Dev 3 | P2 | S |
| Send `Set-Cookie:` headers in responses | Dev 1 | P2 | M |
| Handle session expiration/timeout | Dev 3 | P2 | S |
| Add session integration to dynamic routes | Shared | P2 | M |

___

### Sprint 4 Success Criteria

- New clients receive a `Set-Cookie: session_id=<value>` header.
- Returning clients send valid `Cookie:` and retrieve same session.
- Sessions can store temporary data (e.g., user preferences, small payloads).
- Sessions expire after timeout or inactivity.

___

### Key Deliverables (sprint 4)

- Stateless HTTP session management over cookies.
- Server-side session memory storage.
- Proper cookie setting and parsing.
- Secure session ID generation (randomized).

___

### Constraints and Reminders

- Session IDs must be **reasonably secure** (random, non-predictable).
- Session expiration handling should avoid memory leaks (cleanup expired sessions).
- Keep session storage lightweight (simple in-memory map is sufficient for Webserv).

___

### Mindset Reminder (sprint 4)

> Focus on **clean, modular session management**.
>
> Keep it minimal but solid: no databases, no file-based session persistence needed.

</details>

___

## Webserv Project - Multiple CGI Handlers

<details>
<summary><strong>See Sprint 5</strong></summary>

> Bonus sprint: Allow the server to dynamically handle multiple CGI interpreters based on file extension.

This greatly improves flexibility (running PHP, Python, etc.) and adds extra evaluation points.

___

### Developer Assignments (sprint 5)

#### Dev 3 - Configuration Extension

- Extend `.conf` parser:
  - Allow specifying CGI handlers per file extension.
  - Example:

```nginx
location / {
    cgi_handler .php /usr/bin/php-cgi;
    cgi_handler .py /usr/bin/python3;
}
```

#### Dev 2 - Request Detection

- Update `HttpRequestParser` or `RequestHandler`:
  - Detect when the requested file matches a CGI extension.
  - Select the correct interpreter.

#### Dev 1 - CGI Launcher Update

- Modify CGI execution:
  - Use the appropriate interpreter path based on the file extension.
  - Not hardcoded to only one interpreter.

___

### Tasks Summary (sprint 5)

| Task | Owner | Priority | Size |
|:-----|:------|:---------|:-----|
| Extend config parser to load multiple CGI handlers | Dev 3 | P2 | M |
| Detect CGI extensions in request path | Dev 2 | P2 | M |
| Select appropriate CGI executable dynamically | Dev 1 | P2 | M |
| Fork/exec using the selected interpreter | Dev 1 | P2 | M |
| Add error handling if no handler found for extension | Dev 2 | P2 | S |

___

### Sprint 5 Success Criteria

- Server can run `.php`, `.py`, `.pl` or any configured CGI.
- Correct interpreter selected dynamically at runtime.
- If extension is missing or unknown, server returns 500 error.
- CGI execution remains compliant with Webserv constraints (fork only for CGI).

___

### Key Deliverables (sprint 5)

- Dynamic CGI handler selection based on file extension.
- Flexible server able to run multiple types of scripts.
- Clear error management for invalid/missing CGI setups.

___

### Constraints and Reminders (sprint 5)

- Maintain clean environment setup for each CGI fork (PATH_INFO, QUERY_STRING, etc.).
- Avoid hardcoding assumptions about interpreters.
- Properly handle fork/exec errors (send 500 Internal Server Error).

__

### Mindset Reminder (sprint 5)

> Make your server **adaptable and flexible** to any scripting backend.
>
> This bonus makes your server feel "real" like NGINX or Apache.

</details>
___

## SPRINT 6 - WebSocket Basic Support

> Bonus sprint: Implement a basic **WebSocket** server with RFC 6455 handshake and simple message echoing.

This feature demonstrates modern protocol support and earns significant bonus points if fully functional.

___

### Developer Assignments (sprint 6)

#### Dev 2 - WebSocket Handshake

- Detect WebSocket upgrade requests:
  - Check for `Upgrade: websocket` and `Connection: Upgrade` headers.
- Complete the handshake:
  - Compute and send the correct `Sec-WebSocket-Accept` value.

#### Dev 1 - Frame Parsing and Echo Server

- Implement basic WebSocket frame parsing:
  - Support minimal TEXT frames.
- Implement simple echo server behavior:
  - Receive message frames and send them back.

#### Dev 3 - Configuration (Optional)

- Optionally allow enabling/disabling WebSocket upgrades per route in the config.

___

### Tasks Summary (sprint 6)

| Task | Owner | Priority | Size |
|:-----|:------|:---------|:-----|
| Detect WebSocket upgrade requests | Dev 2 | P2 | M |
| Perform WebSocket handshake correctly | Dev 2 | P2 | M |
| Parse basic WebSocket frames | Dev 1 | P2 | L |
| Send WebSocket text frames (echo back) | Dev 1 | P2 | L |
| (Optional) Enable WebSocket per route in config | Dev 3 | P3 | S |

___

### Sprint 6 Success Criteria

- Server accepts WebSocket upgrades.
- Clients can establish WebSocket connections successfully.
- Server echoes text messages received from clients.
- Basic support for fragmented frames can be skipped for now.

___

### Key Deliverables (sprint 6)

- WebSocket handshake (RFC 6455 compliant).
- Text frame parsing and echoing.
- Clean integration with existing poll() event loop.

___

## Constraints and Reminders (sprint 6)

- WebSocket protocol is persistent: connection remains open.
- Correctly manage client state between HTTP and WebSocket modes.
- Properly close WebSocket connections on client disconnect.

___

### Mindset Reminder (sprint 6)

> Focus on a minimal but stable echo server first.

</details>
___

## SPRINT 7 - Resilience and Rate Limiting

<details>
<summary><strong>See Sprint 7</strong></summary>

> Bonus sprint: Strengthen server robustness under stress, and protect against basic abuse patterns.

This greatly improves stability during load and demonstrates production-level quality.

___

### Developer Assignments (sprint 7)

### Dev 3 - Rate Limiting

- Implement per-client IP rate limiting:
  - Limit new connections or requests per second.
  - Store client IP address and timestamps.

### Dev 1 - Connection Timeout and Cleanup

- Detect idle connections:
  - Track last activity timestamp.
  - Close sockets inactive for a timeout period (e.g., 30 seconds).

### Dev 2 - Slowloris Protection

- Defend against slow POST attacks:
  - Set a per-client body read timeout.
  - If body upload is too slow, terminate the connection.

___

### Tasks Summary (sprint 7)

| Task | Owner | Priority | Size |
|:-----|:------|:---------|:-----|
| Implement per-IP connection rate limiting | Dev 3 | P2 | M |
| Track and timeout idle connections | Dev 1 | P2 | M |
| Detect and kill slow POST body uploads | Dev 2 | P2 | M |
| Add statistics tracking for connection limits (optional) | Shared | P3 | S |

___

### Sprint 7 Success Criteria

- No single IP can flood the server with unlimited connections.
- Idle connections are closed automatically.
- Slowloris-style slow body uploads are detected and terminated.
- Normal clients are unaffected under load.

___

### Key Deliverables (sprint 7)

- Rate-limited new connections per IP.
- Timeout inactive connections cleanly.
- Protect against resource exhaustion by slow uploads.

___

### Constraints and Reminders (sprint 7)

- Keep per-client tracking lightweight (hash maps or vectors).
- Timeout values should be configurable (optional bonus).
- Prioritize minimal CPU/memory overhead.

___

### Mindset Reminder (sprint 7)

> A strong server is not just functional but **resilient**.
>
> Make Webserv survive heavy, slow, or abusive clients without dying.

___
