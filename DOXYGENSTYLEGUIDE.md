# Doxygen Style Guide for Modern C++ Projects (Webserv)

This guide defines how documentation must be written across all files in the Webserv project.
It aligns with the Doxyfile configuration, CI integration, and public documentation tooling.

---

## 0. Documentation Structure

Documentation is automatically generated from:

* `include/`: public headers
* `src/`: implementation files
* `tests/`: documented test helpers
* `README.md`: used as the main page via `USE_MDFILE_AS_MAINPAGE`

All files are parsed recursively, and `.md` files are included with full Markdown rendering.

---

## 1. General Doxygen Practices

* All `.hpp`, `.cpp`, and `.tpp` files must start with a `@file` header
* Use `/** */` for documentation blocks (not `///` or `//`)
* Use English and present tense ("Initializes", "Returns")
* Leave a blank line between `@brief` and `@details`
* Use structured tags (`@param`, `@return`, etc.) consistently
* Prefer complete documentation on all public headers, types, and methods

---

## 2. Mandatory Tags for Public API

Use the following tags in this order:

1. `@brief`: short summary
2. `@details`: long explanation (optional)
3. `@tparam`: template parameters (if any)
4. `@param`: function arguments
5. `@return`: function return value
6. `@retval`: distinct return codes
7. `@throws`: thrown exceptions
8. `@note`: side information or caveats
9. `@warning`: must-know risk
10. `@todo`: pending feature or task

---

## 3. Example Template Documentation

```cpp
/**
 * @brief Allocates memory from an arena.
 *
 * @tparam T Type to allocate.
 * @param size Number of elements.
 * @return Pointer to the allocated memory.
 */
template<typename T>
T* arenaAlloc(std::size_t size);
```

---

## 4. File Header Format

```cpp
/**
 * @file    Server.cpp
 * @brief   Implements the Server class.
 *
 * @details Provides TCP server functionality.
 */
```

---

## 5. Modules and Grouping

Use `@defgroup` and `@ingroup` for logical documentation structure.
These are aliased in the Doxyfile to ensure visibility.

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

Suggested groups:

* `http`: HTTP parsing and routing
* `socket`: socket abstraction
* `config`: configuration and parsing
* `core`: event loop and dispatcher
* `utils`: shared helpers

---

## 6. Class and Member Documentation

```cpp
/**
 * @brief Represents a TCP server.
 *
 * @details Binds to ports and accepts clients.
 * @ingroup core
 */
class Server {
public:
    /// @brief Starts the server loop.
    void start();

private:
    int port_; ///< Port number the server uses.
};
```

---

## 7. Enum and Struct Documentation

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

## 8. Internal or Private Code

Use `@internal` (aliased in Doxyfile) to exclude internal code from public output.

```cpp
/**
 * @internal
 * @brief Parses URL-encoded key-value pairs.
 */
static std::map<std::string, std::string>
parseQueryInternal(const std::string& query);
```

---

## 9. Formatting Rules

* Use `/** */` style for all documentation blocks
* Align `@param`, `@return`, and `@throws` blocks
* Leave exactly one space after each `*`
* Keep line length under 100 columns
* Use present tense and noun phrases for `@brief`

Example:

```cpp
/**
 * @brief Binds a socket to a port.
 *
 * @param port Port to bind.
 * @return `true` if successful.
 */
bool bindSocket(int port);
```

---

## 10. Graphviz and Source Display

The following are enabled by the Doxyfile:

* `HAVE_DOT = YES`: enables all graph generation
* `CALL_GRAPH`, `CALLER_GRAPH`: call/caller relationships
* `SOURCE_BROWSER = YES`: annotated source files in HTML
* `VERBATIM_HEADERS = YES`: headers shown with structure

Install Graphviz to ensure graph rendering.

---

## 11. Final Checklist

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
