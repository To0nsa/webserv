# Pull Request Template

## 📋 Title Format

```bash
<module>: short description
```

Example: `http: support chunked transfer decoding`

---

## 📄 Description

- **What does this PR change?**
- **Why is this change necessary?**
- **Anything special to watch for during review?**
- **If fixing a bug, what was the root cause?**

---

## ✅ Checklist Before Review

- [ ] My branch is up to date with `dev` (`git fetch && git rebase dev`).
- [ ] I followed the [Commit Message Guidelines](../CONTRIBUTING.md#-commit-guidelines).
- [ ] Code builds and runs without errors (`make && ./webserv`).
- [ ] I manually tested this change (curl, browser, scripts).
- [ ] I stress-tested this feature (multi-connection, upload size, etc.).
- [ ] There are no debug prints or commented-out code left.
- [ ] Memory check passed (`valgrind` or `-fsanitize=address` if available).

---

## 🧪 How I Tested

Describe briefly how you tested your change:

Example:

```text
- Sent 1000 GET requests concurrently to test stability.
- Uploaded a 10MB file to check upload handling.
- Verified 404 error page displays correctly.
- Compared headers with NGINX behavior for conformance.
```

---

## 🚨 Known Issues (Optional)

- Any known bugs, limitations, or follow-ups needed?

---

## 📎 Related Issues (Optional)

- Fixes #<issue_number> or Related to #<issue_number> (if using GitHub Issues)

---
