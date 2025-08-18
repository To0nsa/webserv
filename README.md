# webserv

[![Build Status](https://github.com/to0nsa/webserv/actions/workflows/build.yml/badge.svg)](https://github.com/to0nsa/webserv/actions/workflows/build.yml)
[![Docs Status](https://github.com/to0nsa/webserv/actions/workflows/docs.yml/badge.svg?branch=main)](https://to0nsa.github.io/webserv/)
[![License](https://img.shields.io/github/license/to0nsa/webserv.svg)](LICENSE)
[![GitHub Pages](https://img.shields.io/badge/docs-online-blue.svg)](https://to0nsa.github.io/webserv/)
[![clang-tidy](https://img.shields.io/badge/clang--tidy-enabled-brightgreen)](https://clang.llvm.org/extra/clang-tidy/)
[![clang-format](https://img.shields.io/badge/clang--format-automatic-blue)](https://clang.llvm.org/docs/ClangFormat.html)
[![editorconfig](https://img.shields.io/badge/editorconfig-supported-lightgrey)](https://editorconfig.org/)

> A lightweight HTTP/1.1 server written in modern C++20, compliant with the Hive/42 webserv project specifications.

___

## Configuration Parsing Flow

This section describes how the configuration parsing logic of **Webserv** works, including the step‑by‑step pipeline and the rules applied during parsing and validation.

<details>
<summary><strong>See Details</strong></summary>

### 1. Tokenization

* **Component:** `Tokenizer`
* **Goal:** Convert raw configuration text into a structured list of tokens.
* **Steps:**

  * Skip UTF‑8 BOM if present.
  * Ignore whitespace, line breaks, and comments (`# ...`).
  * Classify tokens into categories:

    * **Keywords:** `server`, `location`, `listen`, `host`, `root`, `index`, `autoindex`, `methods`, `upload_store`, `return`, `error_page`, `client_max_body_size`, `cgi_extension`.
    * **Identifiers:** Alphanumeric strings with `-`, `.`, `/`, `:` allowed.
    * **Numbers & Units:** Digits with optional single‑letter suffix (`k`, `m`, `g`).
    * **Strings:** Quoted values (single `'` or double `"`).
    * **Symbols:** `{`, `}`, `;`, `,`.
  * Detect and reject invalid characters, control characters, or malformed identifiers.

### 2. Parsing

* **Component:** `ConfigParser`
* **Goal:** Transform token stream into structured objects (`Config`, `Server`, `Location`).
* **Rules:**

  * **Block structure:** Curly braces `{ ... }` delimit `server` and `location` blocks.
  * **Directives:** Each directive must end with `;` unless it opens a block.
  * **Directive placement:** Certain directives are only valid at specific levels:

    * Server level: `listen`, `host`, `server_name`, `error_page`, `client_max_body_size`.
    * Location level: `root`, `index`, `autoindex`, `methods`, `upload_store`, `return`, `cgi_extension`, `cgi_interpreter`.
  * **Nesting:** Locations may not contain other `server` blocks.

### 3. Configuration Objects

* **Server:** Represents a virtual host.

  * Holds host, port, server names, error pages, body size limits, and `Location` blocks.
* **Location:** Defines behavior for a URI path prefix.

  * Includes root directory, index file(s), autoindex flag, allowed methods, redirects, CGI settings, and upload store.

### 4. Normalization

* After parsing, the configuration is **normalized** to ensure consistency and defaults:

  * Missing `client_max_body_size` → default = **1 MB**.
  * Missing `error_page` → add defaults for common errors (403, 404, 500, 502 → `/error.html`).
  * Missing `methods` → defaults to **GET, POST, DELETE**.
  * Locations without `root` → fallback to `/var/www` (unless redirected).
  * Root location (`/`) without `index` → defaults to **index.html**.
* Normalization guarantees that later validation and runtime logic operate on a **complete and uniform** model.

### 5. Validation

* **Component:** `validateConfig`
* **Goal:** Enforce semantic correctness beyond syntax.
* **Checks applied:**

  * **Presence checks:** At least one `location` per `server`.
  * **Path rules:** Location paths must start with `/` and not contain segments beginning with `.`.
  * **Defaults:** Each location must define either a `root` or `return` (but not both with CGI).
  * **Server names:** Must be unique per host\:port, valid per RFC 1035 (no spaces, no control chars, no empty labels).
  * **Ports:** Only one unnamed default server per host\:port pair.
  * **Error pages:** Codes restricted to 400–599.
  * **Redirects:** Only 301, 302, 303, 307, 308 allowed.
  * **Methods:** Only `GET`, `POST`, `DELETE` permitted.
  * **Client body size:** Must be > 0.
  * **CGI:** Extensions must start with a dot, interpreters must map 1‑to‑1 with declared extensions.
  * **Roots & Upload stores:** Must exist and be directories.
  * **Index:** Requires a valid `root`.

### 6. Error Handling

* **Tokenizer:** Throws `TokenizerError` with line/column context when encountering invalid tokens.
* **Parser:** Throws `ConfigParseError` on invalid structure or misplaced directives.
* **Validator:** Throws `ValidationError` with descriptive guidance on fixing invalid configurations.

</details>

### Flow Overview

1. **Tokenizer** → breaks input into tokens.
2. **ConfigParser** → builds in‑memory `Config` with `Server` & `Location` objects.
3. **normalizeConfig** → fills missing defaults (sizes, error pages, roots, index, methods).
4. **validateConfig** → applies semantic checks.
5. **Runtime** → validated configuration is passed to the server for request routing.

The configuration pipeline guarantees that only syntactically valid, normalized, and semantically correct configurations are accepted. This ensures the server runs with predictable defaults, strong validation, and developer-friendly diagnostics.

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

This project leverages **GitHub Actions** to ensure code quality, stability, and up-to-date documentation.

<details>
<summary><strong>See Details</strong></summary>

### CI Pipeline

* Runs automatically on pushes and pull requests to `main` and `dev`.
* Includes manual triggers (`workflow_dispatch`) and dependency checks after successful builds.

**Jobs Overview:**

| Job          | Description                                                                                      |
| ------------ | ------------------------------------------------------------------------------------------------ |
| 🔨 **Build** | Compiles the project using the provided `Makefile` to ensure successful builds.                  |
| 🧪 **Test**  | Builds the server, runs Python test suite against a live instance, and captures logs on failure. |
| 📚 **Docs**  | Generates Doxygen documentation (with Graphviz diagrams) and deploys it to **GitHub Pages**.     |

</details>

Every code change is built, tested, and documented automatically, ensuring a robust development workflow and always-available reference docs.

___

## Documentation

This section describes how project documentation is generated, structured, and published.

<details>
<summary><strong>See Details</strong></summary>

### 1. Doxygen-Powered

* Documentation is generated automatically from **source code comments** and **Markdown files**.
* `README.md` serves as the **entry point**, offering an overview and links to modules.

### 2. Graphical Support

* **Graphviz** integration produces:

  * **Class diagrams** to illustrate object hierarchies.
  * **Call graphs** to visualize execution flow.
  * **Dependency graphs** to map relationships between modules.
* These visuals improve comprehension of the server’s architecture.

### 3. Navigation & Browsing

* The source browser cross-references **functions, classes, and files**.
* Each documented entity links directly to its definition in the codebase.
* Groups (`@defgroup`, `@ingroup`) provide thematic navigation across modules (e.g., `config`, `core`, `http`).

### 4. Deployment

* Documentation is built in **CI/CD pipelines**.
* Published automatically via **GitHub Pages** from the `docs/html` directory.
* Ensures the latest version is always available for contributors and maintainers.

### 5. Best Practices

* Consistent **Doxygen-style headers** across `.hpp` and `.cpp` files.
* Markdown files complement code documentation with **high-level design notes** and **workflow explanations**.
* Together, these guarantee both **low-level API reference** and **high-level architectural guidance**.

</details>

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
├── .clang-format               # Enforces formatting rules (4-space indent, K&R braces, etc.)
├── .editorconfig               # Shared IDE/editor config for consistent style
├── .gitattributes              # Defines merge/diff rules for Git (e.g. binary files)
├── .gitignore                  # Files and folders ignored by Git (e.g. build/, *.o)
├── ACTIONPLAN.md               # Project-level planning or roadmap
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
