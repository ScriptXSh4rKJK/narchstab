# Changelog

All notable changes to narchstab will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2025-03-08

### Added
- `narchstab oom` — automatic OOM killer configuration for `systemd-oomd` and `earlyoom`
  - Auto-detects available backend (`auto | systemd-oomd | earlyoom | none`)
  - Reads `/proc/meminfo` to display RAM and swap info
  - Writes drop-in config for `systemd-oomd` (`/etc/systemd/oomd.conf.d/narchstab.conf`)
  - Writes sysconfig for `earlyoom` (`/etc/default/earlyoom`)
  - Enables and starts the chosen service via `systemctl`
  - `--dry-run` support — shows what would change without applying
- `narchstab dmesg` — hardware error scanner
  - Parses dmesg output, classifies 35+ patterns across 8 categories:
    MCE, I/O, Memory/ECC, Filesystem, Network, USB, GPU, Kernel Oops
  - Configurable severity threshold (`emerg` → `notice`)
  - Groups findings by category with a summary table
  - Sends notifications for critical/warning issues
- `narchstab report` — failed systemd unit reporter
  - Enumerates all `failed` units via `systemctl list-units`
  - Pulls timestamps, exit codes, and recent journal entries per unit
  - Includes suggested fix commands
  - Output to stdout or file (`report_output = file`)
- Notification system with three independent channels:
  - Log file (always-on, configurable path)
  - Desktop notifications via `notify-send` (libnotify)
  - Telegram bot
- Optional one-line integration with `narchsafe` to inherit Telegram credentials
- Full `--dry-run` mode for `oom` subcommand
- `--verbose`, `--show-config`, `--version`, `--help` flags
- Config file at `/etc/narchstab.conf` with inline-comment support
- PID lock file to prevent parallel runs
- MIT License

### Security
- All child processes spawned with a clean, minimal environment (`SAFE_ENV`)
- All file descriptors marked `O_CLOEXEC` where appropriate
- Temporary files created with `mkstemp()` and explicit `chmod 0600`
- No `system()` calls — all external programs run via `execve()`
- Telegram parameters passed as separate curl args (no string concatenation / injection risk)
- `pipe2(O_CLOEXEC)` used instead of `pipe()` to prevent fd leaks across fork
- OOM backend detection uses `open()+fstat()` instead of `access()` to avoid TOCTOU
- `ensure_dir()` uses `mkdir()+EEXIST` pattern instead of `stat()+mkdir()`
- Lock file PID written with `fsync()` for visibility across processes
- Config files written with `fsync()` before close

[Unreleased]: https://github.com/ScriptXSh4rKJK/narchstab/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/ScriptXSh4rKJK/narchstab/releases/tag/v1.0.0
