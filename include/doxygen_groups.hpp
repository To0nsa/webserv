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
 * @brief    Configuration structures, parsing, normalization, and validation.
 *
 * @details Handles reading, tokenizing, and parsing the Webserv configuration file(s),
 *          validating directives, and preparing normalized configuration objects
 *          for use by the core server.
 */

/**
 * @defgroup config_tokenizing Tokenizing
 * @ingroup  config
 * @brief    Lexical analysis of configuration text into tokens.
 *
 * @details Converts raw configuration input into a token stream (types, values, positions)
 *          consumed by the parser.
 */

/**
 * @defgroup config_parsing Parsing
 * @ingroup  config
 * @brief    Syntactic parsing and directive dispatch.
 *
 * @details Builds in-memory config objects from tokens and applies directive handlers
 *          to populate `Server`/`Location` structures.
 */

/**
 * @defgroup config_parse_error Parsing Errors
 * @ingroup  config
 * @brief    Error types and helpers for tokenizer/parser diagnostics.
 *
 * @details Structured exceptions carrying human-friendly messages and context snippets
 *          for syntax/lexing errors.
 */

/**
 * @defgroup config_normalizing Normalizing
 * @ingroup  config
 * @brief    Post-parse defaulting and canonicalization.
 *
 * @details Fills in default values (methods, error pages, indices, sizes) and
 *          normalizes paths/settings for predictable downstream behavior.
 */

/**
 * @defgroup config_validation Validating
 * @ingroup  config
 * @brief    Configuration validation passes.
 *
 * @details Static checks for structure, duplicates, domain/port uniqueness, allowed methods,
 *          CGI mappings, and filesystem preconditions (roots, upload stores).
 */

/**
 * @defgroup core Core Components
 * @brief Main server components, control flow, and application entry point.
 *
 * @details Contains the event loop, server initialization, and runtime orchestration.
 *          Ties together configuration, networking, and HTTP processing.
 */

/**
 * @defgroup server_component Server Component
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
 * @defgroup location_component Location Component
 * @ingroup core
 * @brief Path-specific routing and configuration within a server.
 *
 * @details Contains the `Location` class, representing a configuration
 *          block bound to a specific URI path inside a server.
 *          Each location can define:
 *          - A document root.
 *          - Allowed HTTP methods.
 *          - Index files and autoindexing.
 *          - Redirection rules.
 *          - CGI execution settings.
 *          - Upload directory configuration.
 *
 *          Files:
 *          - `core/Location.hpp` — class declaration.
 *          - `core/Location.cpp` — method implementations.
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
 * @defgroup socker_mananager Network & Socket Abstraction
 * @brief Non-blocking I/O, socket management, and connection handling.
 *
 * @details Provides abstractions over system calls for listening sockets,
 *          client connections, and readiness-based multiplexing using poll/kqueue/epoll.
 */

/**
 * @defgroup utils Utility Functions
 * @brief Shared helper routines for common operations.
 *
 * @details Provides generic helper functions reused across modules, including
 *          string manipulation, date/time handling, and filesystem operations.
 */

/**
 * @defgroup filesystem_utils Filesystem Utilities
 * @ingroup utils
 * @brief Path manipulation, safe file handling, and directory management.
 *
 * @details Contains helpers for:
 *          - Normalizing and joining paths.
 *          - Mapping URIs to filesystem paths.
 *          - Sanitizing filenames for uploads.
 *          - Creating directories recursively.
 *          - Enforcing upload root boundaries.
 *          These utilities are designed to prevent directory traversal,
 *          enforce security constraints, and support Webserv’s upload and
 *          static file-serving features.
 */

/**
 * @defgroup html_utils HTML Utilities
 * @ingroup utils
 * @brief Safe HTML encoding and related helpers.
 *
 * @details Functions for escaping or manipulating HTML content so that
 *          untrusted input can be embedded safely in a page without being
 *          interpreted as markup. Prevents common injection vulnerabilities
 *          like cross-site scripting (XSS) by replacing reserved characters
 *          with their corresponding HTML entities.
 */

/**
 * @defgroup string_utils String Utilities
 * @ingroup utils
 * @brief String manipulation, parsing, and formatting helpers.
 *
 * @details Provides reusable string-related routines including:
 *          - Case conversion (uppercase/lowercase).
 *          - Whitespace trimming.
 *          - Delimited join operations.
 *          - Size parsing with suffix multipliers (e.g., KiB, MiB).
 *          - Human-readable byte formatting.
 *          - Integer parsing with detailed error reporting.
 *          These utilities are used throughout Webserv for configuration
 *          parsing, logging, and data presentation.
 */

/**
 * @defgroup url_utils URL Utilities
 * @ingroup utils
 * @brief Helpers for percent-decoding, form decoding, and safe filename extraction.
 *
 * @details Provides functions for:
 *          - Decoding percent-encoded sequences in URIs.
 *          - Handling `application/x-www-form-urlencoded` form data.
 *          - Parsing key/value pairs from form bodies.
 *          - Extracting and validating safe filenames from URI segments.
 *
 *          These functions are typically used during HTTP request parsing,
 *          particularly for processing query strings, form submissions,
 *          and safe handling of uploaded filenames.
 */
