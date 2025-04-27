# Modern C++ Coding Style Guide

## 1. Headers and Includes

- Use C++ standard headers only: `<cstring>`, `<string>`, `<vector>`, `<memory>`, `<filesystem>`, etc.
- Include order:
  1. Corresponding header ("MyClass.hpp")
  2. Standard Library headers
  3. External libraries
  4. Project headers

Example:

```cpp
#include "Server.hpp"
#include <vector>
#include <string>
#include <filesystem>
```

## 2. Namespaces

- Do not use `using namespace std;` globally.
- Local `using` is acceptable inside `.cpp` files.

Example:

```cpp
using std::vector;
using std::string;
```

## 3. Class Style

- One class per `.hpp` and `.cpp` file.
- Naming conventions:
  - Classes: PascalCase (`HttpServer`)
  - Methods: camelCase (`startServer`)
  - Members: snake_case (`server_name`, `port`)

Example:

```cpp
class HttpServer {
public:
    void start();
    void stop();

private:
    int port;
    std::string server_name;
};
```

## 4. Smart Pointers

- Prefer `std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr`.

Example:

```cpp
std::unique_ptr<Server> server = std::make_unique<Server>();
```

## 5. Filesystem

- Use `std::filesystem` for paths, files, directories.

Example:

```cpp
std::filesystem::path config_path("config/server.conf");
```

## 6. Function Definitions

- Only prototypes in headers.
- Full definitions in `.cpp`.

Header:

```cpp
class Server {
public:
    void run();
};
```

Source:

```cpp
void Server::run() {
    // implementation
}
```

## 7. Indentation and Braces

- Always use braces, even for one-line blocks.
- K&R style (brace on same line).
- 4 spaces indentation, no tabs.

Example:

```cpp
if (condition) {
    doSomething();
} else {
    doSomethingElse();
}
```

## 8. Const Correctness

- Use `const` for inputs that are not modified.
- Mark methods as `const` if they don't modify the object.

Example:

```cpp
std::string getServerName() const;
void setServerName(const std::string& name);
```

## 9. Orthodox Canonical Form Rules

### What is the Orthodox Canonical Form?

It includes these **five** special member functions:

| Function | Purpose |
|:---------|:--------|
| Default constructor `MyClass()` | Initialize an object |
| Copy constructor `MyClass(const MyClass&)` | Create a copy |
| Copy assignment operator `MyClass& operator=(const MyClass&)` | Assign from another object |
| Destructor `~MyClass()` | Clean up resources |
| (Optionally) Parameterized constructor | Initialize with parameters |

In modern C++, this extends to:

- Move constructor: `MyClass(MyClass&&) noexcept`
- Move assignment: `MyClass& operator=(MyClass&&) noexcept`

**This is often referred to as the Rule of 5.**

---

### When to Explicitly Define It

- Class **manages resources** (raw pointers, file descriptors, sockets).
- Class has **dynamic memory ownership**.
- You need to **delete copy/move** to prevent unwanted copying.
- You want **explicit copy/move behavior**.

Example: Classes that manage a socket, file, or manual heap memory.

---

### When You Can Skip It

- Class only contains **standard containers** (`std::vector`, `std::string`, etc.).
- Class is a **simple data holder** (POD - Plain Old Data).
- Class does not manage any manual resources.

The compiler-generated versions are correct and optimal in these cases.

---

### Best Modern Practices

### Rule of Zero

- Design classes that **don't need custom copy/move/destructor**.
- Rely fully on standard containers and smart pointers.

### Rule of Three

- If you define any of: destructor, copy constructor, or copy assignment operator, **you should define all three**.

### Rule of Five

- If you manually define a destructor, copy/move constructor, or copy/move assignment operator, **define all five**.

---

### Minimalist Canonical Form Templates

#### Trivial Class (No Resource Management)

```cpp
class Config {
public:
    Config() = default;
    ~Config() = default;
    Config(const Config&) = default;
    Config& operator=(const Config&) = default;
    Config(Config&&) noexcept = default;
    Config& operator=(Config&&) noexcept = default;
};
```

#### Resource-Managing Class

```cpp
class ServerSocket {
public:
    explicit ServerSocket(int fd);
    ~ServerSocket();

    ServerSocket(const ServerSocket&) = delete;
    ServerSocket& operator=(const ServerSocket&) = delete;

    ServerSocket(ServerSocket&& other) noexcept;
    ServerSocket& operator=(ServerSocket&& other) noexcept;

private:
    int socket_fd;
};
```

---

### Quick Summary Table

| Situation | Should you write canonical form? | Why? |
|:----------|:-------------------------------|:----|
| Class manages a resource | ✅ Yes | Manual control of copy/move required |
| Class has only `std::vector`, `std::string`, etc. | ❌ No | Compiler-generated is optimal |
| Simple config/data class | ❌ No | Overkill |
| Class must not be copied | ✅ Yes | Delete copy constructor/assignment |

---

### Final Takeaway

- If your class **owns resources**, manage copy/move explicitly.
- If not, let the **compiler generate** everything for you.
- Prefer **`= default`** and **`= delete`** over writing boilerplate code.

---

## 10. Error Handling

- Use exceptions for unrecoverable errors.
- Use `std::optional`, `std::variant`, or `std::expected` for recoverable errors.

Example:

```cpp
std::optional<std::string> readConfig(const std::filesystem::path& path);
```

## 11. Comments

- Use Doxygen style comments for public classes and methods.

Example:

```cpp
/// @brief Starts the server.
/// @details Opens the socket and begins listening for connections.
void start();
```

## 12. Naming Conventions

| Entity | Convention | Example |
|:-------|:-----------|:--------|
| Class/Struct | PascalCase | `HttpRequest` |
| Method | camelCase | `handleRequest()` |
| Variable | snake_case | `timeout`, `port_number` |
| Constant | ALL_CAPS | `DEFAULT_PORT` |
| Enum | PascalCase | `enum class Method` |

## 13. Other Modern Practices

- Prefer `enum class`.
- Prefer `auto` when obvious.
- Use range-based for loops.
- Use `default` and `delete` for special member functions.

Example:

```cpp
Server(const Server&) = delete;
Server& operator=(const Server&) = delete;
```

## Example

### Server.hpp

```cpp
#pragma once

#include <string>
#include <memory>

class Server {
public:
    Server(const std::string& host, int port);
    ~Server();

    void start();
    void stop();
    [[nodiscard]] bool isRunning() const;

private:
    std::string host;
    int port;
    bool running;
};
```

### Server.cpp

```cpp
#include "Server.hpp"

Server::Server(const std::string& host, int port)
    : host(host), port(port), running(false) {}

Server::~Server() = default;

void Server::start() {
    running = true;
    // bind, listen, accept...
}

void Server::stop() {
    running = false;
}

bool Server::isRunning() const {
    return running;
}
```

---

## Final Notes

- Keep interfaces clean (minimal public API).
- No memory leaks (RAII or smart pointers).
- Stay consistent throughout the project.
