# narchstab

**System stability toolkit for Arch Linux.**  
One binary, three subcommands — configure OOM protection, scan for hardware
errors, and report failed systemd units.

[![CI](https://github.com/YOUR_USERNAME/narchstab/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_USERNAME/narchstab/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0-green.svg)](CHANGELOG.md)

---

## The problem

Arch Linux rolls fast. Three things silently break stability between updates:

1. **OOM kills** — the kernel's built-in OOM killer freezes the desktop for
   seconds before acting. `systemd-oomd` and `earlyoom` are better, but
   require manual configuration to actually work well on your hardware.

2. **Hardware errors in dmesg** — MCE events, I/O errors, ECC corrections,
   and kernel oopses get buried in thousands of lines of dmesg output and
   are easy to miss until something fails catastrophically.

3. **Failed systemd units** — services crash silently. You only notice when
   something stops working. `systemctl --failed` gives you a list but no
   context.

`narchstab` fixes all three in one tool.

---

## Features

| Subcommand | What it does |
|---|---|
| `narchstab oom` | Detects available OOM backend, tunes thresholds for your RAM/swap, writes config, enables service |
| `narchstab dmesg` | Scans dmesg for 35+ hardware error patterns across 8 categories, sends notifications on findings |
| `narchstab report` | Lists failed systemd units with journal snippets, timestamps, exit codes and suggested fix commands |

- **Zero runtime dependencies** — pure C11, links only against libc
- **Safe by default** — all subprocesses run via `execve()` with a clean
  minimal environment; no `system()` calls
- **Dry-run mode** — `--dry-run` shows exactly what would change without
  touching anything
- **Three notification channels** — log file, `notify-send` (desktop),
  Telegram bot — each independently toggleable in config
- **Optional narchsafe integration** — set one line in config to inherit
  Telegram credentials from [narchsafe](https://github.com/YOUR_USERNAME/narchsafe);
  no hard dependency

---

## Installation

### From source

```bash
git clone https://github.com/YOUR_USERNAME/narchstab.git
cd narchstab
make
sudo make install
```

This installs:
- `/usr/local/bin/narchstab`
- `/etc/narchstab.conf`  (only if it does not already exist)
- `/var/lib/narchstab/`  (state directory, mode 0750)
- `/var/log/narchstab.log` (mode 0640)

### AUR (coming soon)

```bash
yay -S narchstab
# or
paru -S narchstab
```

### Dependencies

| Dependency | Required | Notes |
|---|---|---|
| `gcc` or `clang` | Build only | C11 compiler |
| `make` | Build only | |
| `systemd` | Runtime | `systemctl`, `journalctl` |
| `util-linux` | Runtime | `dmesg` |
| `earlyoom` | Optional | For `oom_backend = earlyoom` |
| `libnotify` | Optional | For `notify_libnotify = yes` |
| `curl` | Optional | For `notify_telegram = yes` |

---

## Usage

### `narchstab oom` — Configure OOM protection

Detects how much RAM and swap you have, picks the best available backend,
and writes a tuned configuration.

```
$ sudo narchstab oom

[oom-policy] Backend: systemd-oomd
  Memory: 15938 MB
  Swap:   8191 MB
  Wrote: /etc/systemd/oomd.conf.d/narchstab.conf
    SwapUsedLimit              = 90%
    DefaultMemoryPressureLimit = 90%
  Enabled: systemd-oomd.service

[oom-policy] Done.
```

**Dry-run — see what would change without touching anything:**

```
$ sudo narchstab --dry-run oom

[oom-policy] Backend: systemd-oomd
  Memory: 15938 MB
  Swap:   8191 MB
  [DRY-RUN] would write drop-in: /etc/systemd/oomd.conf.d/narchstab.conf
  [DRY-RUN] would enable and start: systemd-oomd.service
```

**Config options:**

```ini
oom_backend        = auto       # auto | systemd-oomd | earlyoom | none
oom_mem_threshold  = 90         # kill when memory used > 90%
oom_swap_threshold = 90         # kill when swap used > 90%
```

---

### `narchstab dmesg` — Scan for hardware errors

Scans the kernel ring buffer and classifies everything it finds.

```
$ narchstab dmesg

==========================================
  dmesg-watch — hardware error scan
==========================================
  Severity threshold : warn
  Since boot         : yes
  Issues found       : 3

  Summary by category:
    I/O / disk error         : 2
    Memory error             : 1

  Detail:
  Severity   Category             Message
  -----------------------------------------------------------------------
  err        I/O / disk error     sda: Read-error on swap-device
  err        I/O / disk error     blk_update_request: I/O error, dev sda
  warn       Memory error         EDAC MC0: 1 CE memory read error
```

If nothing is found:

```
$ narchstab dmesg

==========================================
  dmesg-watch — hardware error scan
==========================================
  Severity threshold : warn
  Since boot         : yes
  Issues found       : 0

  No hardware issues detected. System looks clean.
```

**Detected categories:**

| Category | Examples |
|---|---|
| MCE | Machine Check Exceptions, hardware CPU errors |
| I/O / disk | ATA errors, SCSI errors, NVMe errors, blk_update_request |
| Memory | ECC corrections, EDAC events, Bad page, OOM |
| Filesystem | ext4/btrfs/xfs errors, journal commit errors, corruption |
| Network | Link down, NIC watchdog, NETDEV errors |
| USB | Disconnects, resets, enumeration failures |
| GPU | DRM errors, GPU HANG, amdgpu/nouveau faults |
| Kernel Oops | BUG, Oops, kernel panic, WARNING: CPU, Call Trace |

**Config options:**

```ini
dmesg_severity = warn    # emerg | alert | crit | err | warn | notice
```

---

### `narchstab report` — Failed unit report

```
$ narchstab report

==========================================
  failed-units-report — narchstab v1.0.0
  Generated: 2025-03-08 14:22:07
==========================================

  Failed units: 1

------------------------------------------
  [1/1] bluetooth.service
------------------------------------------
  Description  : Bluetooth service
  Load         : loaded
  Active       : failed (Result)
  Last started : Sat 2025-03-08 13:55:12 CET
  Failed at    : Sat 2025-03-08 13:55:13 CET
  Exit code    : 1

  Recent journal:
    13:55:12 hostname bluetoothd[1234]: Bluetooth daemon 5.72
    13:55:13 hostname bluetoothd[1234]: Failed to obtain handles: No such file or directory
    13:55:13 hostname systemd[1]: bluetooth.service: Main process exited, code=exited, status=1/FAILURE

  Suggested fix:
    systemctl status bluetooth.service
    journalctl -xe -u bluetooth.service
    systemctl restart bluetooth.service

==========================================
  To reset all failed units:
    systemctl reset-failed
  Log: /var/log/narchstab.log
==========================================
```

Save report to a file:

```ini
report_output       = file
report_file         = /var/log/narchstab-report.log
report_journal_lines = 50
```

---

## Configuration

The config file is at `/etc/narchstab.conf`. All settings are optional —
the file ships with annotated defaults.

```ini
# /etc/narchstab.conf

# --- General ---
log_file   = /var/log/narchstab.log
lock_file  = /run/narchstab.lock
state_dir  = /var/lib/narchstab

# --- Optional: inherit Telegram creds from narchsafe (no hard dependency) ---
# narchsafe_conf = /etc/narchsafe.conf

# --- Notifications ---
notify_log         = yes
notify_libnotify   = no
notify_telegram    = no
# telegram_token   = 123456789:AAxxxxxxxxxxxxxxxxxxxxxxxx
# telegram_chat_id = -100xxxxxxxxx

# --- oom subcommand ---
oom_backend        = auto    # auto | systemd-oomd | earlyoom | none
oom_mem_threshold  = 90
oom_swap_threshold = 90

# --- dmesg subcommand ---
dmesg_severity     = warn    # emerg | alert | crit | err | warn | notice

# --- report subcommand ---
report_journal_lines = 30
report_output        = stdout  # stdout | file
report_file          = /var/log/narchstab-report.log
```

Print the active configuration:

```bash
narchstab --show-config
```

---

## Running automatically

### On every login (check for problems)

Add to your shell profile or create a systemd user service:

```bash
# ~/.bashrc or ~/.zshrc
narchstab dmesg
narchstab report
```

### Scheduled via systemd timer

Create `/etc/systemd/system/narchstab-dmesg.service`:

```ini
[Unit]
Description=narchstab dmesg hardware scan

[Service]
Type=oneshot
ExecStart=/usr/local/bin/narchstab dmesg
```

Create `/etc/systemd/system/narchstab-dmesg.timer`:

```ini
[Unit]
Description=Run narchstab dmesg every hour

[Timer]
OnBootSec=2min
OnUnitActiveSec=1h

[Install]
WantedBy=timers.target
```

```bash
sudo systemctl enable --now narchstab-dmesg.timer
```

---

## Command reference

```
narchstab oom              Configure OOM killer
narchstab dmesg            Scan dmesg for hardware errors
narchstab report           Report failed systemd units

Options:
  --config <FILE>          Config file (default: /etc/narchstab.conf)
  --dry-run                Simulate without applying changes
  --verbose                More output
  --show-config            Print active configuration and exit
  -h, --help               Show help
  -v, --version            Print version
```

---

## Security

- All external programs are executed via `execve()` with a hardcoded minimal
  environment — no shell, no `PATH` injection
- Telegram token is never passed on the command line; it goes through curl's
  `--data-urlencode` in a forked child
- Temporary files use `mkstemp()` with explicit `chmod 0600`
- All file descriptors are opened with `O_CLOEXEC`
- Binary is built with `-fstack-protector-strong -fPIE -pie
  -Wl,-z,relro,-z,now,-z,noexecstack`

To report a security vulnerability, see [CONTRIBUTING.md](CONTRIBUTING.md#security-issues).

---

## Related projects

- **[narchsafe](https://github.com/YOUR_USERNAME/narchsafe)** — safe system
  updater for Arch Linux with automatic backup and rollback

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[MIT](LICENSE)
