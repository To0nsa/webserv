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
 * @defgroup core Core Server Logic
 * @brief Main server control flow and application entry point.
 *
 * @details Contains the event loop, server initialization, and runtime orchestration.
 *          Ties together configuration, networking, and HTTP processing.
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

/**
 * @defgroup utils Utilities & Helpers
 * @brief Shared helper functions and utility classes.
 *
 * @details Common helpers for string manipulation, filesystem handling,
 *          logging, and error reporting used across all modules.
 */
