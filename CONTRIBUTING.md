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

> **Note:** CI will automatically open a PR from `dev` to `main` when `dev` is green.

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

- **Bug / Feature Requests** → `.github/ISSUE_TEMPLATE/issue_template.md`
- **Pull Requests** → `.github/PULL_REQUEST_TEMPLATE/pull_request_template.md`

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
- No WIP commits. Clean history first (`git rebase -i`).

---

## Pull Requests

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

- At least **one teammate** must review and approve.
- Address comments seriously; resolve only once addressed.
- Clarify anything that’s ambiguous.

---

### Merging

- Merge after approvals.
- Prefer **Rebase & Merge** or **Squash & Merge** (avoid regular merge commits).
- Delete the feature branch after merging.

---

## Cleanup Rules

- No commented-out code.
- Always run `make clean && make fclean` before pushing.
- Make sure `.gitignore` correctly excludes build artifacts and editor files.

---

## Static Analysis: `clang-tidy`

We enforce best practices with `clang-tidy`.  
Config is in [`/.clang-tidy`](.clang-tidy).

### Key points

- All warnings are errors.
- Enforced naming conventions:
  - `lower_case` for variables
  - `PascalCase` for classes
  - `camelCase` for functions
- Use `nullptr` (not `NULL`).
- Private members must end with `_`.

Run manually:

```bash
clang-tidy <file>.cpp -- -Iinclude
```

Fix every warning before submitting a PR.

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
