---
name: Bug Report
about: Something is broken or behaves unexpectedly
title: "[BUG] "
labels: bug
assignees: ''
---

## Description
A clear description of what the bug is.

## Command run
```
narchstab <subcommand> [flags]
```

## Expected behaviour
What you expected to happen.

## Actual behaviour
What actually happened. Include the full terminal output.

## Log output
```
# paste relevant lines from /var/log/narchstab.log
```

## Environment
- narchstab version: <!-- run: narchstab --version -->
- Arch Linux kernel: <!-- run: uname -r -->
- systemd version:   <!-- run: systemctl --version | head -1 -->
- OOM backend:       <!-- systemd-oomd / earlyoom / none -->
