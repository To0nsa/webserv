# Doxygen Style Guide for Modern C++ Projects (Webserv)

## 1. General Doxygen Practices

- Always document **public classes**, **methods**, **functions**, **enums**, **modules**, and **every file**.
- Every `.hpp`, `.cpp`, and `.tpp` file must start with a Doxygen file header.
- Use English only.
- Use **present tense** for descriptions (e.g., "Parses", "Allocates", "Handles").
- Keep descriptions **short, factual, and action-driven**.
- Prefer **brief** + **details** split for clarity.
- Use `@note`, `@warning`, `@todo`, and `@retval` when relevant.
- Align tags and content neatly when applicable.

Example:

```cpp
/**
 * @brief Starts the server.
 *
 * @details Opens a socket, binds it, and listens for incoming connections.
 * @note This function must be called before accepting clients.
 * @warning Throws an exception if the socket cannot be created.
 * @todo Support IPv6 sockets in future versions.
 */
void start();
```

---

## 2. Mandatory Sections for Public API

Each public function/class should include:

| Tag        | Purpose                         |
|:-----------|:---------------------------------|
| `@brief`   | One-line summary                 |
| `@details` | Longer description if needed     |
| `@param`   | For each parameter               |
| `@return`  | For non-void functions            |
| `@retval`  | For specific return value meanings|
| `@throws`  | If the function may throw         |
| `@note`    | Optional important notes         |
| `@warning` | Optional warnings about usage     |
| `@todo`    | Optional to mark future work      |

Example:

```cpp
/**
 * @brief Parses an HTTP request from a raw string.
 *
 * @details Extracts the HTTP method, headers, and body.
 * @param  raw_request The raw HTTP request string.
 * @return A parsed HttpRequest object.
 * @retval empty_request If the input is empty.
 * @note Expects a complete HTTP request with headers.
 */
HttpRequest parseHttpRequest(const std::string& raw_request);
```

---

## 3. Groups and Modules

- Use `@defgroup` to create logical groups (HTTP, Sockets, Config, Server Core, Utils).
- Use `@ingroup` inside classes and functions to link them to a group.
- **Group identifiers must use snake_case.**

Example:

```cpp
/**
 * @defgroup http HTTP
 * @brief Classes and functions related to HTTP handling.
 * @{ 
 */

/**
 * @ingroup http
 * @brief Represents an HTTP request.
 */
class HttpRequest { /* ... */ };

/** @} */
```

Recommended groups for Webserv:

- `@defgroup http HTTP Protocol Handling`
- `@defgroup socket Socket Management`
- `@defgroup config Server Configuration`
- `@defgroup core Server Core Logic`
- `@defgroup utils Utility Functions`

---

## 4. Private/Internal Code

- Document private methods if they are complex.
- Use `@internal` to mark code hidden from public documentation.

Example:

```cpp
/**
 * @internal
 * @brief Parses a URL-encoded query string.
 */
static std::map<std::string, std::string> parseQueryInternal(const std::string& query);
```

---

## 5. Class Documentation Structure

Document classes like this:

```cpp
/**
 * @brief Represents a TCP server.
 *
 * @details Handles socket creation, listening, and client acceptance.
 * @ingroup core
 */
class Server { /* ... */ };
```

Inside the class:

- Public methods: full Doxygen comments.
- Private members: minimal or no comment unless complex.

---

## 6. Enum and Struct Documentation

Example:

```cpp
/**
 * @brief Lists supported HTTP methods.
 * @ingroup http
 */
enum class HttpMethod {
    GET,    ///< Retrieves a resource.
    POST,   ///< Submits data.
    DELETE  ///< Removes a resource.
};
```

---

## 7. Formatting Rules

- Use `/** */` for all documentation blocks.
- Use `//` only for short inline code comments.
- One space after each `*` in a block.
- No blank line between `@brief` and `@details`.
- Tag order:
  - `@brief`
  - `@details`
  - `@param`
  - `@return`
  - `@retval`
  - `@throws`
  - `@note`
  - `@warning`
  - `@todo`
- Align descriptions neatly when relevant.

---

## 8. File Headers

Every `.hpp`, `.cpp`, and `.tpp` file must start with:

```cpp
/**
 * @file    <filename>
 * @brief   <Short description of the file>
 *
 * @details <Longer explanation of what this file contains or does>
 */
```

Example for `Server.hpp`:

```cpp
/**
 * @file    Server.hpp
 * @brief   Declares the Server class.
 *
 * @details Contains the Server class responsible for handling TCP connections.
 */
```

Example for `Server.cpp`:

```cpp
/**
 * @file    Server.cpp
 * @brief   Implements the Server class.
 *
 * @details Provides method implementations for TCP connection handling.
 */
```

Example for `Server.tpp`:

```cpp
/**
 * @file    Server.tpp
 * @brief   Template implementations for the Server class.
 *
 * @details Defines template methods of the Server class.
 */
```

---

## Final Takeaways

- Document **modules first** with `@defgroup`.
- Use **`@ingroup`** for every public class or function.
- **Every file must have a Doxygen file header.**
- Use `/** */` everywhere except inline `//` comments.
- Prefer and properly use `@note`, `@warning`, `@todo`, and `@retval`.
- **Group identifiers must use snake_case**.
- Use **present tense** in all descriptions.
- Align descriptions cleanly.
- Focus on **public API clarity first**, and document complex internals as needed.

---
