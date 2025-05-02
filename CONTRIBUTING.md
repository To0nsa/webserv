# Contributing to Webserv

Thanks for being part of the team! This document outlines our workflow, standards, and best practices to ensure smooth collaboration.

---

## Branching Strategy

Our Git branching model keeps the codebase **stable**, **collaborative**, and **easy to maintain**.

- **`main`**:  
  Stable, production-ready code only.  
  No direct pushes. Only PRs from `dev` are merged after review and validation.

- **`dev`**:  
  Latest working version.  
  All new features, fixes, and experiments integrate here first.

- **Feature branches**:  
  Created from `dev`.  
  Naming: `feature/<short-description>` or `bugfix/<short-description>`.  
  PRs must target `dev`, not `main`.

> **Note:** TODO CI would automatically open a PR from `dev` to `main` when `dev` is green.

### Typical Workflow

```bash
git checkout dev
git pull origin dev
git checkout -b feature/<feature-name>
# Work on your feature...
git add .
git commit -m "feature: implement basic GET request handling"
git push origin feature/<feature-name>
# Open a PR from feature/<feature-name> → dev
```

---

## 📄 Issues & Pull Requests

We use GitHub templates:

- **[Bug / Feature Requests](.github/ISSUE_TEMPLATE/issue_template)**
- **[Pull Requests](.github/PULL_REQUEST_TEMPLATE/pull_request_template.md)**


### PR Title Convention

```text
<module>: short description
```

Example: `http: support chunked transfer decoding`.

---

## Pre-Push Checklist

- `make` must succeed without warnings.
- `./webserv` must start without errors.
- Run :
  - `make asan`
  - `make tsan`
  - `make ubsan`

Open a PR only when all local tests pass.

---

## Commit Guidelines

Good commits = good collaboration.

### Commit Structure

```text
<module>: short summary (max 50 chars)

Optional longer explanation (wrap at 72 chars).
```

- **Module**: `http`, `server`, `config`, `utils`, etc.
- **Short Summary**: Clear and specific.
- **Longer Explanation** (optional): Extra context, tricky parts, fixes.

### Rules

- Fix commits must mention the bug (`cgi: fix PATH_INFO handling`).
- One commit = one purpose.
- Imperative mode: “implement”, not “implemented”.
- No WIP commits. Before opening a PR, clean up your commit history using: (`git rebase -i origin/dev`).

---

## Pull Requests

### How to do a Pull Request

**1.** Start from dev

```bash
git checkout dev
git pull origin dev
```

**2.** Create your feature branch from `dev`

```bash
git checkout -b feature/my-feature
```

**3.** Work and commit
Make your changes, then commit them following the commit guidelines:

```bash
git add .
git commit -m "module: short, meaningful description"
```
Repeat as needed.

**4.** Push your branch

```bash
git push origin feature/my-feature
```

**5.** Open the Pull Request on GitHub

- Go to your repo on GitHub.
  - You’ll see a prompt: “Compare & pull request” — click it
Or go to Pull Requests → New Pull Request

- Set the base to dev
- Set the compare to feature/my-feature

- Fill in:
  - Title: follow the format: module: short description
  - Description:
    - What was changed
    - Why it was needed
    - Anything specific to review/test

- Submit as a Draft if it's not finished yet, or as a regular PR if it's ready

**6.** After Review

- Make changes if requested.
- Push new commits: they are automatically added to the same PR
- Once approved, you or the reviewer can Squash & Merge the PR into dev

### Opening a PR

- Always target `dev`, **never** `main`.
- Open a **Draft PR** for early feedback.
- Mark **Ready for Review** once finalized.
- Use the PR template.
- Follow commit/PR title conventions.

### A PR must

- Contain one logical change.
- Include a clear title and detailed description:
  - What changed?
  - Why was it needed?
  - Anything special to watch during review.
- Pass local builds/tests.
- Be cleaned up (no debug prints, no commented-out code).
- Be rebased on the latest `dev`.

---

### Code Review

**1.** You open a Pull Request (PR)
- Once you’ve pushed your feature branch and opened a PR targeting dev, it enters review phase.

**2.** Teammates are notified
- Anyone watching the repo or specifically requested as a reviewer gets notified.
- You can explicitly assign reviewers via the PR sidebar.

**3.** Reviewers leave comments
- Reviewers can:
  - Approve the PR
  - Request changes
  - Comment (neutral feedback)

- They can:
  - Leave inline comments on specific lines
  - Leave general comments at the top or bottom
  - Suggest changes with GitHub's suggestion UI
  - Discuss architecture, bugs, naming, clarity, etc.

**4.** You respond to feedback
- You can:
  - Reply to comments
  - Commit fixes and push to the same branch — the PR auto-updates
  - Mark comments as resolved when done
    - GitHub tracks which comments are resolved vs. still active.

**5.** Once approved, you merge
- After:
  - All reviewers approve
  - CI passes
  - You’ve rebased & cleaned history

- Then:
  - Use Squash & Merge or Rebase & Merge
  - Delete the feature branch if no longer needed

#### Tips for Efficient Review
- Be responsive to feedback
- Keep changes small and scoped
-  Avoid "monster PRs"
- Use Draft PRs early to gather feedback
- Address every comment, even if just to explain why no change is needed
- At least **one teammate** must review and approve.
- Clarify anything that’s ambiguous.

---

## Merge Strategies: Rebase & Merge vs Squash & Merge

When collaborating on GitHub, choosing the right merge strategy affects your repository's history clarity, bisectability, and maintainability. Here's a clear comparison between **Rebase & Merge** and **Squash & Merge**, with guidance for when to use each.

---

### Rebase & Merge

#### What it does

* Replays each commit from the PR branch **on top of** the target branch (e.g., `dev`).
* **Preserves** all individual commits.
* Avoids merge commits.

#### Resulting history

```
Before:
main --- A --- B
              \
               C --- D  (feature)

After:
main --- A --- B --- C' --- D'  (C and D rebased on B)
```

#### Pros and Cons

| Pros                                         | Cons                                                    |
| -------------------------------------------- | ------------------------------------------------------- |
| Clean, linear history                        | Rewrites commit SHAs (not suitable for shared branches) |
| Preserves logical commit structure           | Requires clean, well-structured commits                 |
| Great for detailed `git blame` and bisecting |                                                         |

#### Use when

* The PR has clean, meaningful commits
* You want to preserve granular history
* Your team follows good commit hygiene

#### Example Workflow

```bash
git checkout dev
git pull origin dev
git checkout -b feature/add-auth
echo "// implement auth" > auth.cpp
git add auth.cpp
git commit -m "auth: implement token-based authentication"
git push origin feature/add-auth
# Open a PR to dev
# After review, rebase interactively to clean history:
git rebase -i dev
git push --force-with-lease
# On GitHub: Use "Rebase & Merge" to merge the PR
```

---

### Squash & Merge

#### What it does

* Squashes **all PR commits into one**.
* Commits that were WIP, fixups, etc., are flattened.

#### Resulting history

```
Before:
main --- A --- B
              \
               C --- D  (feature)

After:
main --- A --- B --- E  (one squashed commit)
```

#### Pros and Cons

| Pros                                 | Cons                                        |
| ------------------------------------ | ------------------------------------------- |
| Extremely clean and concise history  | Loses individual commit information         |
| Easy to revert a single change       | Harder to trace how a feature was developed |
| Great when PR contains noisy commits |                                             |

#### Use when

* PR has WIP/fix commits
* You care more about trunk clarity than commit history
* You want one commit per feature/fix

#### Example Workflow

```bash
git checkout dev
git pull origin dev
git checkout -b bugfix/fix-upload-path
echo "// fix path bug" > upload.cpp
git add upload.cpp
git commit -m "fix: wrong upload directory"
echo "// debug" >> upload.cpp
git commit -am "debug: temporary logging"
echo "// cleanup" >> upload.cpp
git commit -am "cleanup: remove debug"
git push origin bugfix/fix-upload-path
# Open a PR to dev
# On GitHub: Use "Squash & Merge" and write a clean commit message
```

---

### Summary Table

| Feature                      | Rebase & Merge       | Squash & Merge |
| ---------------------------- | -------------------- | -------------- |
| Preserves individual commits | Yes                  | No             |
| Cleans messy commit history  | No                   | Yes            |
| Easy to revert feature       | Maybe (many commits) | Yes (1 commit) |
| Great for `git blame`        | Yes                  | Depends        |
| Suitable for WIP commits     | No                   | Yes            |
| Maintains linear history     | Yes                  | Yes            |

---

### Best Practices

* Rebase locally to keep up-to-date: `git pull --rebase`
* Clean up commits before PR: `git rebase -i`
* Use squash for small fixes, doc updates, or messy PRs
* Use rebase when you want full commit fidelity

---

### Suggested Strategy for `webserv`

| PR Type           | Suggested Merge |
| ----------------- | --------------- |
| Clean feature PR  | Rebase & Merge  |
| Bugfix with noise | Squash & Merge  |
| Docs/Chores       | Squash & Merge  |

Maintain a balance: clear main branch history with squash, but keep fidelity where it helps future devs understand changes.


---

### Cleanup Rules

- No commented-out code.
- Always run `make clean && make fclean` before pushing.
- Make sure `.gitignore` correctly excludes build artifacts and editor files.

---

## clang-tidy Usage and Best Practices

`clang-tidy` is a powerful static analysis tool for C++ that helps catch bugs, enforce coding standards, and suggest modern best practices. It integrates cleanly into CI/CD pipelines and local development workflows.

---

### What is clang-tidy?

`clang-tidy` is part of the LLVM/Clang toolchain. It performs static analysis on C++ code using a set of configurable checks. It supports:

* Bug detection (memory issues, undefined behavior, etc.)
* Style enforcement (naming conventions, brace usage, etc.)
* Modernization (C++11–C++20 idioms)
* Performance improvements (loop transformations, unneeded copies)

---

### Project Integration

In the Webserv project:

* We enforce best practices via `clang-tidy`
* Configuration is located in the root of the repository: [`.clang-tidy`](.clang-tidy)

---

### Configuration Overview (`.clang-tidy`)

```yaml
Checks: >
  -*,
  bugprone-*,
  modernize-*,
  performance-*,
  readability-*,
  clang-analyzer-*,
  cppcoreguidelines-*,
  misc-*

WarningsAsErrors: '*'

HeaderFilterRegex: '.*'

FormatStyle: file

CheckOptions:
  - key:             modernize-use-nullptr.NullMacros
    value:           'NULL'
  - key:             modernize-use-override.CheckSpelling
    value:           true
  - key:             readability-identifier-naming.VariableCase
    value:           lower_case
  - key:             readability-identifier-naming.ClassCase
    value:           PascalCase
  - key:             readability-identifier-naming.FunctionCase
    value:           camelCase
  - key:             readability-identifier-naming.PrivateMemberSuffix
    value:           _
```

---

### Enforced Rule Categories

The configuration above enables the following major rule sets:

* **`bugprone-*`**: Detects code that is likely to be incorrect or prone to bugs (e.g., uninitialized values, copy-paste errors).
* **`modernize-*`**: Promotes modern C++ usage (e.g., `nullptr`, `override`, range-based loops, smart pointers).
* **`performance-*`**: Highlights inefficient code patterns and suggests optimized alternatives (e.g., unnecessary copies).
* **`readability-*`**: Improves clarity and style consistency (e.g., naming, magic numbers, redundant expressions).
* **`clang-analyzer-*`**: Static path-sensitive analysis to detect dead code, memory leaks, and undefined behavior.
* **`cppcoreguidelines-*`**: Enforces the C++ Core Guidelines to encourage safe, maintainable, and modern C++.
* **`misc-*`**: A miscellaneous set of useful checks not covered by other categories.

In addition:

* **All warnings are treated as errors**
* **Naming conventions** are strictly enforced

---

### Naming Conventions Enforced

`clang-tidy` enforces identifier naming across your codebase using the `readability-identifier-naming` checks. The enforced rules include:

| Entity Type     | Convention   | Example                |
| --------------- | ------------ | ---------------------- |
| Variables       | `lower_case` | `timeout`, `buffer_id` |
| Functions       | `camelCase`  | `handleRequest()`      |
| Classes/Structs | `PascalCase` | `HttpServer`, `Config` |
| Private Members | `_` suffix   | `connection_`, `port_` |

These conventions help maintain a consistent, readable codebase. Violations will be flagged and must be corrected before submission.

---

### How to Run clang-tidy Locally

#### Prerequisites

* `clang-tidy` installed (typically via `clang-tools` or `llvm` package)
* Compilation database (`compile_commands.json`) in your build directory

Generate the compilation database (with CMake):

```bash
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S . -B build
```

Or if using CMake presets:

```bash
cmake --preset debug
```

Then symlink or copy it to project root:

```bash
ln -s build/compile_commands.json .
```

#### Manual Run

```bash
clang-tidy src/Server.cpp -- -Iinclude
```

> Use `-- -Iinclude` to pass include paths to the compiler properly.

#### Run on all files (parallel)

```bash
find src/ -name '*.cpp' | xargs -P$(nproc) -n1 clang-tidy -- -Iinclude
```

---

### Fixing Issues

Use fix mode to auto-apply some suggestions:

```bash
clang-tidy src/Server.cpp -fix -- -Iinclude
```

Always review changes with `git diff` before committing.

---

### CI Integration

GitHub Actions automatically runs `clang-tidy` on pushes to `dev` and `main`. If any issue is found:

* CI will fail
* You must fix all warnings before merging or submitting a PR

Use `clang-tidy` locally to catch issues early.

---

### IDE Integration

Using an IDE that supports `clang-tidy` greatly improves your workflow:

### Visual Studio Code (VSCode)

* Requires extensions: `clangd` or `LLVM`, and optionally `CodeLLDB`
* Make sure `compile_commands.json` is generated
* Set up `c_cpp_properties.json` or use `clangd` with `--compile-commands-dir`
* Configure diagnostics via `.clang-tidy`

### Other Editors

* Most modern C++ IDEs (Eclipse CDT, Qt Creator, etc.) can integrate with `clang-tidy`
* Look for settings to point to `.clang-tidy` and the compilation database

Benefits of IDE integration:

* Instant feedback while coding
* Fix suggestions as you type
* Fewer CI surprises
* Encourages early cleanup

---

### Development Tips

* Use `.clang-tidy` overrides via command line: `-checks=*,-some-check`
* For language version issues: `--extra-arg=-std=c++20`
* Add `-p build` if `compile_commands.json` is in a subdirectory
* Use `clang-tidy -export-fixes fixes.yaml` to export fix suggestions

---

### Summary

* `clang-tidy` enforces consistency and quality
* All warnings are treated as errors
* Used in CI and should be part of local workflow
* IDE integration makes it faster and easier to use
* Configured to match the Webserv style guide
* Covers bug-prone patterns, modern C++ practices, performance, readability, and naming
* Enforces strict identifier naming conventions to ensure clean and uniform code

---

For full documentation: [https://clang.llvm.org/extra/clang-tidy/](https://clang.llvm.org/extra/clang-tidy/)


---

## Code Style

Follow the **[Modern C++ Style Guide](STYLEGUIDE.md)** based on **LLVM style**, adapted for us:

- 4 spaces indentation, no tabs.
- Max 100 columns.
- K&R braces (`if (...) {`).
- `*` and `&` bind to type (`int* ptr`).
- Includes sorted alphabetically but logical grouping preserved.
- No `using namespace std;` at file scope.
- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) over raw pointers.
- Follow Rule of 0 / Rule of 5.
- Prefer exceptions and `std::optional` / `std::expected` for errors.
- Doxygen comments for all public entities.

---

### Documentation

The project documentation is automatically generated using **Doxygen** and deployed to **GitHub Pages**:

- [📚 View Online Documentation](https://to0nsa.github.io/webserv/)

Documentation is generated from the `include/`, `src/`, and `tests/` directories, and can be regenerated manually using Doxygen.

- [📄 Local Documentation Guide](docs/README.md) — Explains the structure, regeneration process, and GitHub Pages deployment.

> Documentation includes call graphs and caller graphs if [Graphviz](https://graphviz.gitlab.io/) is installed.

#### Documentation Style

All public classes, functions, and modules must follow the project's [Doxygen Style Guide](DOXYGENSTYLEGUIDE.md).

This guide defines:

- Mandatory sections (`@brief`, `@details`, `@param`, `@return`, etc.)
- File headers for all `.hpp`, `.cpp`, and `.tpp` files
- Logical module grouping (`@defgroup`, `@ingroup`)
- Formatting rules and tag order
- Use of present tense and alignment of tags
- Handling of private/internal documentation (`@internal`)

---

## Editor Configuration

We ship an [`.editorconfig`](.editorconfig):

- UTF-8 encoding
- 4-space indentation
- Unix (LF) line endings
- End files with newline
- No trailing whitespace (Markdown exempt)
- 100 characters recommended line limit

Configure your editor accordingly.

---

## Git Attributes

Our [`.gitattributes`](.gitattributes) ensures:

- LF line endings everywhere
- Correct binary file handling (e.g., `.png`, `.jpg`, `.gif`)

---

## Git Ignore

Our [`.gitignore`](.gitignore) excludes:

- Build artifacts (`build/`, `objs/`, `bin/`)
- Compiler outputs (`*.o`, `*.out`, `*.exe`, `*.a`)
- IDE configs (`.vscode/`, `.idea/`, `*.swp`)
- OS files (`.DS_Store`, `Thumbs.db`)
- Logs (`*.log`)

**Never** commit ignored files.

---
