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
