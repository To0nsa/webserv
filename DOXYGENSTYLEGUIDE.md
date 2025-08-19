# Doxygen Style Guide for Modern C++ Projects (Webserv)

This guide defines **mandatory documentation rules** for the Webserv project. It is strict by design to ensure consistent, maintainable, and navigable documentation across the entire codebase.

---

## 0. Scope & Enforcement

* Applies to **all** `.hpp`, `.cpp`, `.tpp`, `.ipp` files in `include/`, `src/`, `tests/`.
* Public API **must** be fully documented.
* Pull requests will be **rejected** if:

  * Any public class/function is undocumented.
  * Tag order or format does not follow this guide.
  * File headers are missing.

---

## 1. General Practices

* Always use `/** */` for documentation blocks (never `///` or `//`).
* Write in **English** and **present tense** (e.g., "Initializes", not "Initialized").
* Leave **one blank line** between `@brief` and `@details`.
* Keep **line length < 100 columns**.
* Use **noun phrases** for `@brief`.
* Document **private/internal methods** when behavior is non-obvious (mark with `@internal`).

---

## 2. Tag Order (Mandatory)

1. `@brief` – short summary (≤ 1 sentence)
2. `@details` – extended description (optional)
3. `@tparam` – template parameters
4. `@param` – parameters (aligned vertically)
5. `@return` – return value
6. `@retval` – distinct return codes (optional)
7. `@throws` – exceptions thrown
8. `@note` – additional info
9. `@warning` – important risk
10. `@todo` – pending task/feature

---

## 3. File Header Format

```cpp
/**
 * @file    Server.cpp
 * @brief   Implements the Server class.
 *
 * @details Provides TCP server functionality for Webserv.
 * @ingroup core
 */
```

* **Must** be the first comment in the file.
* Include `@ingroup` for module classification.

---

## 4. Modules & Grouping

Use `@defgroup` and `@ingroup` to organize documentation.

```cpp
/**
 * @defgroup http HTTP Protocol
 * @brief All HTTP-related classes and helpers.
 * @{ */

/**
 * @ingroup http
 * @brief Represents an HTTP request.
 */
class HttpRequest {};

/** @} */
```

**Allowed groups:**

* `http` – HTTP parsing and routing
* `socket` – socket abstraction
* `config` – configuration and parsing
* `core` – event loop and dispatcher
* `utils` – shared helpers

---

## 5. Class & Member Documentation

```cpp
/**
 * @brief Represents a TCP server.
 *
 * @details Binds to ports and accepts clients.
 * @ingroup core
 */
class Server {
public:
    /** @brief Starts the server loop. */
    void start();

private:
    int port_; ///< Port number used by the server.
};
```

* Use `///<` for **inline member comments**.
* Use `/** */` for **methods**.

---

## 6. Function Documentation

```cpp
/**
 * @brief Binds a socket to a port.
 *
 * @param port Port to bind.
 * @return `true` if successful.
 * @throws std::runtime_error If binding fails.
 */
bool bindSocket(int port);
```

* All params **must** be documented.
* If exceptions are thrown, **must** include `@throws`.

---

## 7. Template & Concepts Documentation

For C++20 concepts or constrained templates:

```cpp
/**
 * @brief Allocates memory from an arena.
 *
 * @tparam T Type to allocate.
 * @param size Number of elements.
 * @return Pointer to allocated memory.
 */
template<typename T> requires std::is_default_constructible_v<T>
T* arenaAlloc(std::size_t size);
```

---

## 8. Enum & Struct Documentation

```cpp
/**
 * @brief Supported HTTP methods.
 * @ingroup http
 */
enum class HttpMethod {
    GET,    ///< Retrieves a resource.
    POST,   ///< Submits data.
    DELETE  ///< Deletes a resource.
};
```

---

## 9. Internal/Private Code

```cpp
/**
 * @internal
 * @brief Parses URL-encoded key-value pairs.
 */
static std::map<std::string, std::string>
parseQueryInternal(const std::string& query);
```

* Mark with `@internal` to exclude from public output.

---

## 10. Cross-Referencing

Use `@ref` to link related docs:

```cpp
/// See also: @ref bindSocket
```

---

## 11. Final Pre-Merge Checklist

Before merging:

* [ ] Each file has a `@file` header

* [ ] All public classes and methods are documented

* [ ] All parameters and return values are explained

* [ ] Internal helpers use `@internal`

* [ ] All tags are ordered and formatted

* [ ] Logical modules are defined and grouped

* [ ] No undocumented public entities remain

---

By following this guide, you ensure clean, maintainable, and navigable documentation across the Webserv codebase.
