# Modern C++ Coding Style Guide (Enhanced for clang-tidy)

This style guide defines the conventions used throughout the Webserv project. All rules are enforced by `.clang-tidy`, `.clang-format`, and GitHub Actions CI.

---

## 1. Naming Conventions

| Entity               | Convention    | Example           |
| -------------------- | ------------- | ----------------- |
| Variable             | `snake_case`  | `buffer_size`     |
| Function             | `camelCase`   | `handleRequest()` |
| Class / Struct       | `PascalCase`  | `HttpServer`      |
| Enum                 | `PascalCase`  | `HttpMethod`      |
| Enum Constant        | `UPPER_CASE`  | `GET`, `POST`     |
| Constant (constexpr) | `UPPER_CASE`  | `DEFAULT_TIMEOUT` |
| Private Member       | `_snake_case` | `_connection`     |

Naming rules are enforced by `clang-tidy` via the `readability-identifier-naming` check group. Violations will fail CI.

---

## 2. File Structure & Naming

* **Source files**: `snake_case.cpp`
* **Header files**: `snake_case.hpp`
* **One class per file**
* Match filename with class name (e.g. `HttpServer.hpp` ↔ `HttpServer.cpp`)

---

## 3. Includes and Order

1. Corresponding header
2. Standard library
3. Third-party libraries
4. Project headers

Example:

```cpp
#include "Server.hpp"
#include <vector>
#include <string>
#include "utils/config_utils.hpp"
```

---

## 4. Indentation and Braces

* Indent with 4 spaces (no tabs)
* Use **K\&R style**:

```cpp
if (x) {
    doSomething();
} else {
    doSomethingElse();
}
```

* Always use braces, even for one-liners

---

## 5. Pointer and Reference Style

* Attach `*` and `&` to the **type**:

```cpp
int* ptr;
std::string& name = ref;
```

---

## 6. Const Correctness

* Use `const` everywhere applicable:

```cpp
std::string getName() const;
void setTimeout(const int timeout);
```

---

## 7. Canonical Form

Follow the Rule of 0 / Rule of 5:

* Use `= default` / `= delete`
* Define all 5 special functions only if managing a resource manually

Example:

```cpp
class HttpServer {
public:
    HttpServer() = default;
    ~HttpServer() = default;
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&&) noexcept = default;
    HttpServer& operator=(HttpServer&&) noexcept = default;
};
```

---

## 8. Constants and Macros

* Prefer `constexpr` or `const` over macros
* Use `UPPER_CASE` for global constants

```cpp
constexpr int MAX_CONNECTIONS = 100;
```

* Avoid `#define` unless platform-level or header guards

---

## 9. Enums

* Use `enum class` (strongly typed)
* Name with `PascalCase`, constants in `UPPER_CASE`

```cpp
enum class HttpMethod {
    GET,
    POST,
    DELETE
};
```

---

## 10. Error Handling

* Use `std::optional`, `std::variant`, or exceptions
* Never silently fail

---

## 11. Comments

* Doxygen-style for all public API
* Use present tense, factual, action-oriented descriptions

Example:

```cpp
/* @brief Starts the server.
 * @details Opens the socket and begins listening.
 * void start();
 */
```

---

## 12. Tools and Enforcement

* Naming and structural conventions are enforced by `.clang-tidy`
* Formatting is enforced via `.clang-format`
* CI fails on any `clang-tidy` or format violation
* Follow C++ Core Guidelines (`cppcoreguidelines-*` in clang-tidy)

---

## 13. Examples

### Good

```cpp
class HttpRequest {
public:
    void parseHeaders();

private:
    int content_length_;
};
```

### Bad

```cpp
class http_request {
    int ContentLength;
    void Parseheaders();
};
```

---

## References

* [https://clang.llvm.org/extra/clang-tidy/](https://clang.llvm.org/extra/clang-tidy/)
* [https://clang.llvm.org/docs/ClangFormat.html](https://clang.llvm.org/docs/ClangFormat.html)
* [https://isocpp.github.io/CppCoreGuidelines/](https://isocpp.github.io/CppCoreGuidelines/)
