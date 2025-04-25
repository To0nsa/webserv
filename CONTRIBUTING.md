# Contributing to Webserv

Thanks for being part of the team. We are going to tackle **Webserv** with ease and tears. This document outlines our workflow, standards, and best practices to ensure smooth collaboration.

---

## 🧩 Branching Strategy

Our Git branching model is designed to keep the codebase **stable**, **collaborative**, and **easy to maintain**.

- **`main` branch:**
  - Contains only stable and production-ready code.
  - We never push directly to main.
  - Every push to main must come from a fully reviewed and tested Pull Request (PR) from dev.

- **`dev` branch:**
  - This is where feature branches are merged once they are complete.
  - `dev` always represents the latest working version of the project, but not necessarily production-ready.
  - New features, bug fixes, and experiments are integrated into `dev` first.

- **Feature branches:**
  - Create a new branch from `dev` whenever you start working on a new feature, fix, or improvement.
  - Name it using the pattern: `feature/<short-description>` or `bugfix/<short-description>`.
  - Examples:
    - feature/http-parser
    - feature/file-upload
    - bugfix/request-timeout
  - After completing your work, submit a PR **into** `dev`, not main.

### 📈 Typical Workflow

```bash
# Always start by updating your local dev
git checkout dev
git pull origin dev

# Create a new branch for your feature
git checkout -b feature/<feature-name>

# Work on your feature...

# Stage, commit, and push your changes
git add .
git commit -m "feature: implement basic GET request handling"
git push origin feature/<feature-name>

# Open a Pull Request from feature/<feature-name> → dev
```

### 🚨 Important Rules

- **Never** commit directly to main or dev.
- **One branch = one logical** change (don't mix unrelated fixes and features).
- **Keep branches short-lived:** merge them into dev once finished and reviewed.
- **Rebase or pull `dev` often** to avoid big merge conflicts.
- **If in doubt,** ask in the discussion before starting a big feature.

---

## 🔨 Commit Guidelines

Clear commit messages make it easier to understand the history of the project and speed up code reviews.

### 📋 Commit Message Structure

Each commit should have two parts:

```text
<module>: short summary (max 50 characters)

Optional longer explanation (wrap text at 72 characters per line).
```

- `<module>`: The part of the project affected (http, server, config, utils, socket, cgi, parser, error_handling, main, etc...)
- **Short Summary:** Clear, concise description of what changed.
- **Longer Explanation:** (Optional) More context — WHY you did it, what was tricky, if it fixes something.

### 🚨 Extra Rules

- **Fix commits** must mention the bug clearly (cgi: fix PATH_INFO handling not just fix bug).
- **One commit = one purpose** (don't mix "fix upload" + "add GET" in one commit).
- **Write in imperative form:** “implement”, not “implemented”.
- **No WIP commits:** Always clean your commits before pushing (use git rebase -i if needed).

---

## 🔁 Pull Requests

A Pull Request (PR) is a formal request to merge your code into `dev`.  
Follow these rules to keep the project clean, stable, and easy to review.

### 📥 When opening a PR:
- Always target `dev`, **never `main`**.
- Push your feature or fix branch (`feature/<name>`, `bugfix/<name>`) to GitHub.
- Open a **Draft PR** if your work is still in progress, or you need early feedback.
- Open a **Ready for Review** PR when the feature/fix is complete and tested.

### 📜 PR Title Format:

```bash
<module>: short description
```

Examples:
- `http: support chunked transfer decoding`
- `server: handle multiple listening ports`
- `config: validate upload size limits`

---

### ✅ A Pull Request must:
- Contain **one logical change** (feature or fix) — no mixed commits.
- Include a clear title and description explaining:
  - What you changed
  - Why it was needed
  - Anything special to watch out for during review
- Pass local tests:
  - `make && ./webserv` should build and run without error.
  - Manual or scripted tests must pass (curl, browser, etc.).
- Be cleaned up:
  - No commented-out code
  - No debug prints
  - No "WIP" commits
- Follow the [Commit Message Guidelines](#-commit-message-guidelines-detailed).
- Be rebased on top of the latest `dev` to avoid conflicts (`git rebase dev`).

---

### 🔎 Code Review Process:
- Every PR must be reviewed and approved by at least **one teammate**.
- Fix all comments or discuss them before merging.
- Use "Resolve conversation" only when you actually fixed the point.
- Be open to feedback — code review is about improving quality, not criticizing people.

---

### 🚀 Merging the PR:
- Only merge after all reviews are approved.
- Always use **"Rebase and merge"** or **"Squash and merge"** to keep history clean (never "Merge commit" unless necessary).
- After merge, delete the feature branch if no longer needed.

---

### 🚨 Common PR Mistakes to Avoid:
- PR that is too big (split large features into smaller PRs if possible)
- Missing tests for new code
- Unclear or empty PR description
- Pushing broken or non-compiling code
- Mixing refactors, fixes, and features in one PR

---

## 🗂️ File Naming

- Following 42 rules.

---

## 🧹 Cleanup

- Don’t commit commented-out code.
- Use `make clean && make fclean` before pushing.
- Double-check `.gitignore` excludes binaries, core dumps, editor files.

---
