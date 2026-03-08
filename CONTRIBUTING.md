# Contributing to narchstab

Thank you for considering a contribution. This document explains how to get
started, what standards the code follows, and how to submit changes.

---

## Table of Contents

- [Getting Started](#getting-started)
- [Code Style](#code-style)
- [Building and Testing](#building-and-testing)
- [Submitting a Pull Request](#submitting-a-pull-request)
- [Reporting Bugs](#reporting-bugs)
- [Feature Requests](#feature-requests)
- [Security Issues](#security-issues)

---

## Getting Started

1. Fork the repository and clone your fork:
   ```bash
   git clone https://github.com/YOUR_USERNAME/narchstab.git
   cd narchstab
   ```

2. Make sure the build succeeds before making any changes:
   ```bash
   make
   ```

3. Create a feature branch:
   ```bash
   git checkout -b feat/your-feature-name
   ```

---

## Code Style

The project is written in **C11**. Please follow these conventions:

- **Indentation:** 4 spaces, no tabs.
- **Line length:** 90 characters soft limit, 120 hard limit.
- **Naming:**
  - Functions and variables: `snake_case`
  - Macros and constants: `UPPER_CASE`
  - Types (structs, enums): `PascalCase`
- **Headers:** use `#pragma once`, not include guards.
- **Error handling:** every syscall that can fail must check its return value.
- **Memory:** every `malloc`/`calloc` must be checked for `NULL`.
- **No `system()`:** use `execve()` via the `proc_run*` helpers instead.
- **No `sprintf`/`strcpy`:** use `snprintf`/`memcpy` with explicit sizes.
- **File descriptors:** mark with `O_CLOEXEC` at creation time.

### Compiler checks

All code must compile without warnings under:

```
-std=c11 -Wall -Wextra -Wpedantic -Wformat=2 -Wformat-security
-Wshadow -Wstrict-prototypes -Wdouble-promotion -Wnull-dereference
-O2 -fstack-protector-strong -fPIE
```

Run the full check locally:
```bash
make clean && make CC=gcc
make clean && make CC=clang   # if clang is available
```

---

## Building and Testing

### Build

```bash
make          # release build
make clean    # remove build artifacts
```

### Dry-run smoke test (no root required)

```bash
./narchstab --dry-run oom
./narchstab dmesg
./narchstab report
```

### Install / uninstall

```bash
sudo make install
sudo make uninstall
```

---

## Submitting a Pull Request

1. **One logical change per PR.** Split unrelated fixes into separate PRs.
2. **Update `CHANGELOG.md`** under `[Unreleased]` describing what changed.
3. **Update `README.md`** if you add or change user-facing behaviour.
4. **Ensure CI passes** — the GitHub Actions workflow runs on every push.
5. Write a clear PR description:
   - What problem does this solve?
   - How was it tested?
   - Any known limitations?

---

## Reporting Bugs

Open a [GitHub Issue](https://github.com/YOUR_USERNAME/narchstab/issues) and
include:

- `narchstab --version` output
- Arch Linux kernel version (`uname -r`)
- systemd version (`systemctl --version`)
- The full command you ran and its output
- Relevant lines from `/var/log/narchstab.log`

---

## Feature Requests

Open an issue with the label **enhancement** and describe:

- The problem you are trying to solve
- Your proposed solution or interface
- Any alternatives you considered

---

## Security Issues

**Do not open a public issue for security vulnerabilities.**

Please report them privately by emailing the maintainer or using
[GitHub private vulnerability reporting](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing/privately-reporting-a-security-vulnerability).

Include:
- A description of the vulnerability
- Steps to reproduce
- Potential impact
- Suggested fix (if any)

We aim to respond within 72 hours.
