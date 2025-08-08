# Tasks To Be Done

## 1. Pre-check During Config Load
To make your server more robust:

- Check that all root paths exist at startup
- Warn or refuse to launch if any required root directory is missing

Example startup log:

```
[WARNING] Root directory /home/nlouis/webserv/test_webserv/serverfiles/html/YoupiBanane does not exist.
[ERROR] Server configuration invalid – refusing to start.
```

## 2. Makefile
Remove wildcards and unused targets: 
- `debug`
- `debug_asan` 
- `debug_tsan`
- `debug_ubsan`
- `release`
- `?run`
- `?test`
- `sanitize`
- `fast`
- `help`
- `prepare_dirs`

## 3. Check Usage of Forbidden Functions
Based on the subject, we can use:

**Everything in C++**

**C functions only:**
- `execve`, `dup`, `dup2`, `pipe`, `strerror`, `gai_strerror`
- `errno`, `dup`, `dup2`, `fork`, `socketpair`, `htons`, `htonl`
- `ntohs`, `ntohl`, `select`, `poll`, `epoll` (`epoll_create`, `epoll_ctl`, `epoll_wait`)
- `kqueue` (`kqueue`, `kevent`)
- `socket`, `accept`, `listen`, `send`, `recv`, `chdir`, `bind`
- `connect`, `getaddrinfo`, `freeaddrinfo`, `setsockopt`
- `getsockname`, `getprotobyname`, `fcntl`, `close`, `read`
- `write`, `waitpid`, `kill`, `signal`, `access`, `stat`, `open`
- `opendir`, `readdir`, `closedir`

## 4. File Organization
Organize config files and test files in directories separated from the source code.

