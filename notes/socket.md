## Socket Programming in C — Detailed Explanation

This document provides a detailed explanation of commonly used socket functions in C, including flags and usage examples.

---

### 1. `socket()`

**Purpose:** Creates a new socket (an endpoint for communication).

```c
int socket(int domain, int type, int protocol);
```

**Parameters:**
- `domain`: Address family
  - `AF_INET` — IPv4
  - `AF_INET6` — IPv6
  - `AF_UNIX` — Unix domain sockets
- `type`: Type of socket
  - `SOCK_STREAM` — TCP
  - `SOCK_DGRAM` — UDP
  - `SOCK_RAW` — Raw socket
- `protocol`: Protocol to use (usually `0` for default)

**Returns:** File descriptor, or `-1` on error.

---

### 2. `struct sockaddr_in`

**Purpose:** Stores IPv4 address and port.

```c
struct sockaddr_in {
    sa_family_t    sin_family;   // Address family (AF_INET)
    in_port_t      sin_port;     // Port number (network byte order)
    struct in_addr sin_addr;     // IP address
    char           sin_zero[8];  // Padding (unused)
};
```

**Initialization Example:**
```c
struct sockaddr_in addr;
addr.sin_family = AF_INET;
addr.sin_port = htons(8080); // host to network byte order
addr.sin_addr.s_addr = inet_addr("127.0.0.1");
memset(addr.sin_zero, 0, sizeof(addr.sin_zero));
```

**Helper Functions:**
- `htons()`: Converts port from host to network byte order
- `inet_addr()`: Converts IPv4 address from string to binary (deprecated, use `inet_pton()`)

---


### 3. `bind()`

**Purpose:** Binds a socket to an IP address and port.

```c
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

**Parameters:**
- `sockfd`: Socket descriptor
- `addr`: Address structure (usually `struct sockaddr_in` cast to `struct sockaddr *`)
- `addrlen`: Size of the address structure

**Usage:** Required for servers to specify which IP/port to listen on.

---

### 4. `listen()`

**Purpose:** Marks a socket as passive (ready to accept incoming connections).

```c
int listen(int sockfd, int backlog);
```

**Parameters:**
- `sockfd`: Socket descriptor
- `backlog`: Number of pending connections queue

**Usage:** Only used with TCP (stream) sockets.

---

### 5. `accept()`

**Purpose:** Accepts an incoming connection on a listening socket.

```c
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

**Parameters:**

- `sockfd`: Listening socket descriptor
- `addr`: Pointer to a `sockaddr` structure to receive the client's address
- `addrlen`: Pointer to a variable with the size of `addr`; updated with actual size

**Returns:** New socket descriptor for the accepted connection, or `-1` on error.

**Usage:** Used by servers after `listen()` to accept client connections.

---

### 6. `recv()`

**Purpose:** Receives data from a socket.

```c
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

**Parameters:**
- `sockfd`: Socket descriptor
- `buf`: Buffer to store received data
- `len`: Max number of bytes to receive
- `flags`: Optional behavior modifiers

**Flags:**
- `0`: Default
- `MSG_PEEK`: Peek at incoming data without removing it
- `MSG_WAITALL`: Wait until full data is received
- `MSG_DONTWAIT`: Non-blocking mode
- `MSG_OOB`: Out-of-band data

---

### 7. `connect()`

**Purpose:** Initiates a connection on a socket (used by clients).

```c
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

**Parameters:**
- `sockfd`: Socket descriptor
- `addr`: Remote address
- `addrlen`: Size of address structure

**Returns:** `0` on success, `-1` on error.

---

### 8. `send()`

**Purpose:** Sends data on a connected socket.

```c
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
```

**Parameters:**
- `sockfd`: Socket descriptor
- `buf`: Data to send
- `len`: Number of bytes
- `flags`: Optional behavior modifiers

**Flags:**
- `0`: Default
- `MSG_DONTROUTE`: Send directly to interface
- `MSG_OOB`: Out-of-band data
- `MSG_NOSIGNAL`: Avoid SIGPIPE
- `MSG_MORE`: More data to send later (TCP)

---


---

### Server Program (`server.cpp`)

```cpp
// server
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

int main()
{
    // creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying the address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // binding socket.
    bind(serverSocket, (struct sockaddr*)&serverAddress,
         sizeof(serverAddress));

    // listening to the assigned socket
    listen(serverSocket, 5);

    // accepting connection request
    int clientSocket
        = accept(serverSocket, nullptr, nullptr);

    // receiving data
    char buffer[1024] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);
    cout << "Message from client: " << buffer
              << endl;

    // sending response back to client
    const char* response = "Hello, client! Message received.";
    send(clientSocket, response, strlen(response), 0);

    // closing the socket.
    close(serverSocket);

    return 0;
}
```

### Client Program (`client.cpp`)

```cpp
// client
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    // creating socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // sending connection request
    connect(clientSocket, (struct sockaddr*)&serverAddress,
            sizeof(serverAddress));

    // sending data
    const char* message = "Hello, server!";
    send(clientSocket, message, strlen(message), 0);

    // receiving response from server
    char buffer[1024] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);
    cout << "Message from server: " << buffer
              << endl;

    // closing socket
    close(clientSocket);

    return 0;
}
```
