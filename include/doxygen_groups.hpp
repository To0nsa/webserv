/**
 * @file    doxygen_groups.hpp
 * @brief   Declares main Doxygen documentation groups for Webserv modules.
 *
 * @details This header contains only `@defgroup` declarations.
 *          It is not included in the build for logic, only for documentation purposes.
 *          All source files should use `@ingroup <groupname>` to link to these groups.
 */

/**
 * @defgroup config Configuration Parsing
 * @brief Configuration structures, parsing, normalization, and validation.
 *
 * @details Handles reading, tokenizing, and parsing the Webserv configuration file(s),
 *          validating directives, and preparing normalized configuration objects
 *          for use by the core server.
 */

/**
 * @defgroup core Core Components
 * @brief Main server components, control flow, and application entry point.
 *
 * @details Contains the event loop, server initialization, and runtime orchestration.
 *          Ties together configuration, networking, and HTTP processing.
 */

/**
 * @defgroup server Server Component
 * @ingroup core
 * @brief Main Server class and its implementation.
 *
 * @details Contains the `Server` class, which represents a single
 *          virtual server block in Webserv.
 *
 *          Files:
 *          - `core/Server.hpp` — class declaration.
 *          - `core/Server.cpp` — method implementations.
 *          - `core/server_utils.hpp` — findMatchingServer()
 *          - `core/server_utils.cpp` — findMatchingServer()
 */

/**
 * @defgroup entrypoint Application Entrypoint
 * @ingroup core
 * @brief Program entry and top-level orchestration.
 *
 * @details Contains the `main()` function and the high-level startup/shutdown
 *          logic for Webserv, including:
 *          - `runWebserv.hpp` — high-level declarations.
 *          - `runWebserv.cpp` — main orchestration functions.
 *          - `main.cpp` — program entry point.
 */

/**
 * @defgroup http HTTP Protocol
 * @brief HTTP request/response parsing and handling.
 *
 * @details Includes classes and functions for parsing HTTP requests,
 *          building HTTP responses, routing, and applying HTTP rules.
 */

/**
 * @defgroup network Network & Socket Abstraction
 * @brief Non-blocking I/O, socket management, and connection handling.
 *
 * @details Provides abstractions over system calls for listening sockets,
 *          client connections, and readiness-based multiplexing using poll/kqueue/epoll.
 */
