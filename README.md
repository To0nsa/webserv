# webserv

[![Build Status](https://github.com/to0nsa/webserv/actions/workflows/ci.yml/badge.svg)](https://github.com/to0nsa/webserv/actions/workflows/ci.yml)
[![Docs Status](https://github.com/to0nsa/webserv/actions/workflows/docs.yml/badge.svg?branch=main)](https://to0nsa.github.io/webserv/)
[![License](https://img.shields.io/github/license/to0nsa/webserv.svg)](LICENSE)
[![GitHub Pages](https://img.shields.io/badge/docs-online-blue.svg)](https://to0nsa.github.io/webserv/)
[![clang-tidy](https://img.shields.io/badge/clang--tidy-enabled-brightgreen)](https://clang.llvm.org/extra/clang-tidy/)
[![clang-format](https://img.shields.io/badge/clang--format-automatic-blue)](https://clang.llvm.org/docs/ClangFormat.html)
[![editorconfig](https://img.shields.io/badge/editorconfig-supported-lightgrey)](https://editorconfig.org/)

> A lightweight HTTP/1.1 server written in modern C++20, compliant with the Hive/42 webserv project specifications.

___

## Tokenizer: The First Step of Configuration Parsing

### Overview

The tokenizer is the **first phase** of the configuration file processing pipeline. It transforms the raw text input (usually from a `.conf` file) into a structured list of **tokens** that represent the syntactic elements of the configuration language. These tokens are then consumed by the parser to build the in-memory configuration tree used by the web server.

This separation of concerns improves error reporting, modularity, and testability.

___

### Responsibilities of the Tokenizer

The tokenizer (also known as the lexer) is responsible for:

* **Skipping** irrelevant input:

  * Whitespace (spaces, tabs, carriage returns)
  * Newlines (including CRLF and LF endings)
  * Single-line and multi-line comments (`#`, `//`, `/* ... */`)

* **Lexical classification**:

  * **Keywords** such as `server`, `location`, `listen`, `root`, `methods`, etc.
  * **Identifiers**, which include user-defined names, path fragments, MIME types, and hostnames
  * **Numbers**, including optional unit suffixes (`k`, `m`, `g`) for sizes
  * **Quoted strings**, both single- and double-quoted, with escape sequence handling
  * **Symbols**: structural elements like `{`, `}`, `;`

* **Error detection and diagnostics**:

  * Reports **unterminated strings** with clear context
  * Rejects **invalid escape sequences** inside strings
  * Validates identifiers (no control characters, must be non-empty)
  * Verifies that keywords are lowercase only
  * Detects and reports **unexpected characters** or malformed input early

* **Position tracking**:

  * Maintains line and column numbers for every token
  * Allows precise and informative error messages downstream in the parser

* **Efficiency considerations**:

  * Uses a single linear pass through the input string
  * Reserves memory for tokens to reduce reallocations

The final token returned is always `END_OF_FILE`, which marks the logical end of the input.

___

### Example Tokenization

Input:

```nginx
server {
    listen 8080;
    root "/var/www/html";
    # This is a comment
}
```

Output tokens (via `debugToken()`):

```bash
[Token type=KEYWORD_SERVER value="server" line=1 column=1]
[Token type=LBRACE value="{" line=1 column=8]
[Token type=KEYWORD_LISTEN value="listen" line=2 column=5]
[Token type=NUMBER value="8080" line=2 column=12]
[Token type=SEMICOLON value=";" line=2 column=16]
[Token type=KEYWORD_ROOT value="root" line=3 column=5]
[Token type=STRING value="/var/www/html" line=3 column=10]
[Token type=SEMICOLON value=";" line=3 column=26]
[Token type=RBRACE value="}" line=5 column=1]
[Token type=END_OF_FILE value="" line=6 column=1]
```

Each line shows the exact representation of a token as returned by the `debugToken()` utility, which formats both the token type (via `debugTokenType()`) and its source metadata.

___

### What the Tokenizer Does *Not* Do

The tokenizer **does not**:

* Validate the grammar (e.g., `server` blocks must contain `listen`)
* Resolve scope/nesting of blocks (`{`/`}`)
* Enforce semantic constraints (e.g., port number validity, method duplication)
* Resolve inheritance or merging of locations and directives

These are handled in the **parser phase**, which consumes the token stream and builds the configuration tree.

___

### Benefits of Tokenization

* Early failure on malformed input
* Improved debuggability through token inspection
* Simplified parser logic by cleanly separating concerns
* Reusability: the same tokenizer can be reused in config linters, validators, or autocompletion tools

___

### Related Files

* `Tokenizer.cpp`, `Tokenizer.hpp`: Implementation of the lexer
* `token.cpp`, `token.hpp`: Definition of token types and utilities
* `ConfigParseError.hpp`: Exception types thrown on malformed input

___

## Parser: Building the Configuration Tree

### Overview

The parser is the **second stage** of the configuration pipeline. After the tokenizer has broken the raw configuration file into discrete tokens, the parser consumes this stream to build a structured, validated in-memory representation of the configuration—composed of `Config`, `Server`, and `Location` objects. This tree determines how the server behaves at runtime.

Where the tokenizer is concerned only with syntax, the parser enforces the **grammar**, **semantic rules**, and **hierarchical structure** of the configuration language.

___

### Responsibilities of the Parser

The `ConfigParser` performs the following high-level tasks:

* **Top-level validation**: Ensures the file starts with one or more `server` blocks and that no unexpected tokens appear outside them.

* **Recursive block parsing**: Parses each `server { ... }` block into a `Server` object, and each `location /path { ... }` block into a `Location` object nested inside its respective server.

* **Directive dispatching**: Uses centralized handler tables to map directive names (e.g., `listen`, `root`, `index`) to functions that apply them to configuration objects, validating arguments along the way.

* **Duplicate detection**: Tracks encountered directives per block to reject duplicates, except for explicitly repeatable directives like `error_page` and `methods`.

* **Error reporting**: Provides detailed, context-rich error messages for invalid directives, unexpected tokens, missing semicolons, or block misnests—highlighting the exact token and surrounding snippet.

___

### Grammar Example

This input:

```nginx
server {
    listen 8080;
    host 127.0.0.1;

    location /files {
        root /var/www/data;
        index index.html;
        methods GET POST;
    }
}
```

Produces an internal tree:

```txt
Config
└── Server
    ├── listen: 8080
    ├── host: 127.0.0.1
    └── Location /files
        ├── root: /var/www/data
        ├── index: index.html
        └── methods: GET, POST
```

___

### Design Highlights

* **Directive handler tables**: Each directive is mapped to a specific function (e.g. `"listen" → setPort()`), with argument validation for type, count, and range.

* **Grammar enforcement**: Enforces valid directives per block type, with required braces and semicolons.

* **Normalized directive names**: Converts directive names to lowercase to ensure case-insensitive matching.

* **Position-aware error reporting**: All parser errors include precise line and column numbers, plus a source code snippet—enabling accurate debugging and testing.

___

### What the Parser Does *Not* Do

The parser **does not**:

* Bind sockets or allocate runtime resources
* Validate path existence or filesystem access
* Perform runtime merging of virtual hosts (this is done later)

___

### Benefits of Modular Parsing

* Easy to extend: new directives can be added by inserting a new handler
* Error-resilient: clear diagnostics with minimal overhead
* Testable: parser logic is decoupled from runtime behaviors

___

### Related Files

**Parser Core**:

* `ConfigParser.cpp`, `ConfigParser.hpp`: Parser implementation
* `directive_handler_table.cpp`, `directive_handler_table.hpp`: Directive dispatchers

**Configuration Models**:

* `Config.cpp`, `Config.hpp`: Root configuration object
* `Server.cpp`, `Server.hpp`: Virtual host configuration model
* `Location.cpp`, `Location.hpp`: Path-specific configuration blocks

**Support Code**:

* `token.cpp`, `token.hpp`: Token types and debug printers
* `ConfigParseError.hpp`: Error types for tokenizer and parser phases

___

## Config Normalizer: Applying Safe Defaults

### Overview

The `ConfigNormalizer` is the **third stage** of the configuration pipeline, run immediately after parsing but before validation. Its role is to **apply safe defaults** to optional configuration fields so that the validator can assume the config is complete.

This mirrors the behavior of NGINX and ensures that runtime logic does not need to handle partial or missing configuration.

___

### Responsibilities of the Normalizer

The normalizer visits all `Server` and `Location` objects and fills in missing optional values according to project conventions and HTTP server best practices.

It does **not** attempt to correct invalid values—only to fill in omitted ones.

#### Server-Level Defaults

| Directive              | Default value                      |
|------------------------|------------------------------------|
| `client_max_body_size` | `1MB`                              |
| `error_page`           | 500, 404, 403, 502 → `/error.html` |

#### Location-Level Defaults

| Directive | Default value |
| --- | ---- |
| `root`    | `/var/www`    |
| `index`   | `index.html`  |
| `methods` | `["GET"]`     |

These defaults ensure that every server has valid behavior even if the user provides a minimal configuration.

___

### Example Normalization

Input config:

```nginx
server {
    listen 8080;
    location / {
        root /srv/www;
    }
}
```

After normalization:

```txt
Server:
  client_max_body_size: 1048576 (1MB)
  error_pages:
    500 → "/error.html"
    404 → "/error.html"
    403 → "/error.html"
    502 → "/error.html"

Location "/":
  root: "/srv/www"
  index: "index.html"
  methods: ["GET"]
```

___

### Why Normalize Before Validation?

The validator assumes all fields are complete. Normalizing beforehand:

* Ensures **correct validator behavior** (e.g., `methods` is never empty)
* Prevents **duplicate logic** in runtime error handling
* Matches real-world server behavior (like NGINX)

___

### Related Files

* `ConfigNormalizer.cpp`, `ConfigNormalizer.hpp`: Normalization logic
* `Server.cpp`, `Server.hpp`: Default value holders and setters
* `Location.cpp`, `Location.hpp`: Path-specific config normalization
* `ConfigValidator.cpp`: Validates required structure after defaults are filled

___

___

## Build & Test Instructions

### Build with Makefile

```bash
make
./bin/webserv configs/default.conf
```

Available Makefile targets:

### Build Modes

| Command           | Description                                           |
|----|----|
| `make`            | Build in release mode (optimized)                     |
| `make debug`      | Build in debug mode (with `-g` and no optimizations)   |
| `make debug_asan` | Build in debug mode with AddressSanitizer            |
| `make debug_ubsan`| Build in debug mode with UndefinedBehaviorSanitizer  |
| `make fast`       | Fast build without dependency tracking (development only) |

### Code quality

| Command      | Description                                     |
|-----|----|
| `make format`   | Format all `.cpp` and `.hpp` files using `clang-format` |

### Run and Test

| Command      | Description                                     |
|-----|----|
| `make run`   | Build and run the web server                    |
| `make test`  | Build and run all test binaries from `tests/` folder |
| `make sanitize` | Build and run under all sanitizers (ASAN, TSAN, UBSAN) |

### Cleaning

| Command      | Description                    |
|-----|----|
| `make clean` | Remove all object files and dependency files |
| `make fclean`| Remove everything: binaries, builds, tests |
| `make re`    | Full clean and rebuild          |

### Help

| Command      | Description                    |
|-----|---|
| `make help` | Displays a categorized list of all available `Makefile` targets |

___

## Continuous Integration & Documentation

> This project uses **GitHub Actions** to automate building, testing, and documentation deployment.

### ✅ CI Pipeline

On each push or pull request to `main` or `dev`, the following jobs are run automatically:

| Job                             | Purpose                                                        |
|---|---|
| 🧪 Build (Release)               | Builds the project using the provided `Makefile`.             |
| 📄 Doxygen Docs                  | Generates and deploys Doxygen documentation to GitHub Pages.  |

All configurations rely on the project `Makefile` and follow the project's coding style.

### 🧼 Sanitizer Suppressions

To reduce noise in sanitizer reports, the `.asanignore` file suppresses:

- Known benign leaks from `libstdc++`, `libc`, and dynamic allocators
- Internal race conditions in `__sanitizer` symbols
- Undefined behavior in standard library internals

These help CI focus on bugs *in your code*, not external sources.

### 📚 Documentation

- Doxygen generates HTML docs from source code and Markdown (`README.md` is the main page)
- Graphviz is enabled for call graphs, class diagrams, and source browser
- Documentation is deployed automatically via GitHub Pages from the `docs/html` directory

___

## Contributing

Contribution guidelines and workflow standards are detailed in the dedicated document:

- [📚 View Contributing Guide](CONTRIBUTING.md)

This document explains:

- The coding style
- The branching strategy (main, dev, feature branches)
- The commit message conventions (module: short description)
- How to structure pull requests properly
- The review and merge process
- Cleanup and quality rules before pushing code

___

## Project Structure Overview

```bash
webserv
├── 📁 .github/               # GitHub Actions CI workflows and PR/issue templates
│   └── workflows/
│       ├── ci.yml             # CI workflow: builds with Makefile
│       └── docs.yml           # Doxygen documentation generation & GitHub Pages deploy
├── 📁 include/                # All public project headers, grouped by module (config, http, core, etc.)
├── 📁 src/                    # Source files, mirrors the include/ structure
├── 📁 tests/                  # Unit tests for various modules
├── 📁 configs/                # Test configuration files for parser/tokenizer
├── 📁 docs/                   # Markdown documentation (DOCS.md, guides, etc.)
├── 📁 scripts/                # Helper scripts to run tests and sanitizer builds
├── .asanignore                 # Suppression rules for AddressSanitizer (e.g. libc++ internals)
├── .clang-format               # Enforces formatting rules (4-space indent, K&R braces, etc.)
├── .editorconfig               # Shared IDE/editor config for consistent style
├── .gitattributes              # Defines merge/diff rules for Git (e.g. binary files)
├── .gitignore                  # Files and folders ignored by Git (e.g. build/, *.o)
├── ACTIONPLAN.md               # Project-level planning or roadmap
├── CONTRIBUTING.md             # Guidelines for contributing to the project
├── DOXYGENSTYLEGUIDE.md        # Doxygen conventions for documenting code
├── Doxyfile                    # Main config for Doxygen documentation generation
├── LICENSE                     # Project license (e.g. MIT, GPL)
├── Makefile                    # Build system entry point (defines targets like all, clean, fclean)
├── README.md                   # Main README shown on GitHub (overview, build, usage, etc.)
├── STYLEGUIDE.md               # Coding conventions for naming, layout, formatting
├── webserv.subject.pdf         # Original subject specification for the project
```

___

## License

This project is licensed under the terms of the [MIT License](LICENSE).

___
