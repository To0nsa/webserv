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

> Mandatory for Hive/42 evaluation.

```bash
make
./bin/webserv configs/default.conf
```

Available Makefile targets:

### Build Modes

| Command           | Description                                           |
|-------------------|-------------------------------------------------------|
| `make`            | Build in release mode (optimized)                     |
| `make debug`      | Build in debug mode (with `-g` and no optimizations)   |
| `make debug_asan` | Build in debug mode with AddressSanitizer            |
| `make debug_tsan` | Build in debug mode with ThreadSanitizer              |
| `make debug_ubsan`| Build in debug mode with UndefinedBehaviorSanitizer  |
| `make fast`       | Fast build without dependency tracking (development only) |

### Run and Test

| Command      | Description                                     |
|--------------|-------------------------------------------------|
| `make run`   | Build and run the web server                    |
| `make test`  | Build and run all test binaries from `tests/` folder |
| `make sanitize` | Build and run under all sanitizers (ASAN, TSAN, UBSAN) |

### Cleaning

| Command      | Description                    |
|--------------|---------------------------------|
| `make clean` | Remove all object files and dependency files |
| `make fclean`| Remove everything: binaries, builds, tests |
| `make re`    | Full clean and rebuild          |

___

### Optional: Build with CMake (for GitHub Actions / CI)

For development and automation, a `CMakeLists.txt` with presets is provided.

First, configure the project:

```bash
cmake --preset debug
```

Then build:

```bash
cmake --build --preset debug
```

Executables are placed inside the `build/<config>/bin/` folder (e.g., `build/debug/bin/webserv`).

### Available CMake Presets

| Preset    | Description                                        |
| --------- | -------------------------------------------------- |
| `debug`   | Debug build (with debug symbols, no optimizations) |
| `release` | Optimized build for production                     |
| `asan`    | Debug build with AddressSanitizer (memory bugs)    |
| `tsan`    | Debug build with ThreadSanitizer (thread races)    |
| `ubsan`   | Debug build with UndefinedBehaviorSanitizer (UB)   |

To use another preset, just replace `debug`:

```bash
cmake --preset asan
cmake --build --preset asan
./build/asan/bin/webserv configs/default.conf
```

CMake presets make building easier and ensure consistent builds across machines and CI pipelines.

</details>

___

## Continuous Integration & Documentation

> This project uses GitHub Actions for automated build, test, and documentation deployment.

### CI Pipeline

The following jobs are executed automatically on each push or pull request to the `main` or `dev` branches:

| Job Name                   | Purpose                                           |
|-----------------------------|---------------------------------------------------|
| Build and Test (Debug)      | Build in debug mode and run all tests.            |
| Build and Test (AddressSanitizer) | Build with AddressSanitizer (memory bug detection). |
| Build and Test (UndefinedBehaviorSanitizer) | Build with UndefinedBehaviorSanitizer (UB detection). |
| Build and Test (ThreadSanitizer)  | Build with ThreadSanitizer (thread race detection). |
| Static Analysis (clang-tidy)       | Run static code analysis using `clang-tidy`.        |

All builds are configured using **CMake presets** to ensure reproducibility.

**Static Analysis:** `clang-tidy` is automatically run on each commit to catch potential issues early.  
**Memory Checking:** Builds are tested with **ASAN**, **UBSAN**, and **TSAN** enabled in CI.

### Sanitizer Suppressions

To ensure clean and actionable sanitizer reports, suppression rules are used:

- Ignore harmless leaks from system libraries (`libc`, `libstdc++`, `malloc`, etc.).
- Ignore race conditions inside sanitizer internals (`__sanitizer`).
- Ignore undefined behavior triggered inside standard libraries (e.g., `glibc`).

This minimizes false positives and focuses on real issues inside the project code.

___

## Build System

The project uses a modern **CMake** setup:

- A clean `CMakeLists.txt` is provided with modular includes (`CompilerOptions`, `Warnings`, `Sanitizers`, etc.).
- `compile_commands.json` is automatically generated for tooling (`clang-tidy`, `IDE`, `LSP`).
- All executables are placed in the `bin/` folder for easy access.
- [CMakePresets.json](CMakePresets.json) defines multiple configurations (`debug`, `release`, `asan`, `tsan`, `ubsan`) for easy builds across platforms.
- Multi-configuration generators (Visual Studio, Xcode) are properly handled.

### CMake Modules

This project includes custom CMake modules to enforce code quality, testing, and build robustness:

| Module                  | Purpose                                                |
|--------------------------|--------------------------------------------------------|
| `CompilerOptions.cmake`  | Enforces strict compiler settings and best practices.  |
| `Warnings.cmake`         | Enables a comprehensive set of compiler warnings.      |
| `ClangTools.cmake`       | Integrates clang-tidy and clang-format automatically.  |
| `Sanitizers.cmake`       | Provides easy toggles for ASAN, TSAN, UBSAN.            |
| `CTestSettings.cmake`    | Configures testing behavior with CTest.                 |

> These modules are used automatically when building via **CMake presets** or during **CI builds**.
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
├── .asanignore            # Suppressions for AddressSanitizer
├── .clang-format          # Code formatting rules (LLVM style)
├── .clang-tidy            # Static analysis rules (clang-tidy config)
├── .editorconfig          # Editor settings (indentation, charset, etc.)
├── .gitattributes         # Enforce LF endings, binary handling
├── .gitignore             # Ignore build artifacts, OS files, etc.
├── CMakePresets.json      # CMake build configuration presets
├── CMakeLists.txt         # CMake build script
├── Makefile               # Mandatory 42 Makefile (plus extensions)
├── CONTRIBUTING.md        # Contribution guidelines
├── DOXYGENSTYLEGUIDE.md   # Doxygen documentation rules
├── Doxyfile               # Doxygen config for documentation generation
├── LICENSE                # MIT License file
├── README.md              # Main project documentation
├── STYLEGUIDE.md          # Modern C++ code style guide
├── webserv.subject.pdf    # Official project subject/specification
│
├── cmake/                 # CMake helper modules
│   ├── CTestSettings.cmake
│   ├── ClangTools.cmake
│   ├── CompilerOptions.cmake
│   ├── Sanitizers.cmake
│   └── Warnings.cmake
│
├── configs/               # Example configuration files
│   └── default.conf
│
├── docs/                  # Project documentation
│   └── README.md
│
├── include/               # Header files (empty for now)
│   ├── config/
│   ├── http/
│   ├── server/
│   └── utils/
│
├── scripts/               # Development and test helper scripts
│   ├── run_all_tests.sh
│   ├── run_webserv.sh
│   ├── run_webserv_asan.sh
│   ├── run_webserv_tsan.sh
│   └── run_webserv_ubsan.sh
│
├── src/                   # Source files (empty for now)
│   ├── config/
│   ├── http/
│   ├── server/
│   └── utils/
│
└── tests/                 # Unit and integration tests (empty for now)
    └── .gitkeep
```

___

## License

This project is licensed under the terms of the [MIT License](LICENSE).

___
