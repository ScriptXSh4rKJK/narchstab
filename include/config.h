#pragma once

#define NS_VERSION   "1.0.0"
#define PROG_NAME    "narchstab"

/* Default paths */
#define DEFAULT_CONF_FILE       "/etc/narchstab.conf"
#define DEFAULT_LOG_FILE        "/var/log/narchstab.log"
#define DEFAULT_LOCK_FILE       "/run/narchstab.lock"
#define DEFAULT_DMESG_STATE     "/var/lib/narchstab/dmesg.cursor"
#define DEFAULT_STATE_DIR       "/var/lib/narchstab"

/* Telegram */
#define DEFAULT_TELEGRAM_API_URL "https://api.telegram.org"

/* OOM defaults */
#define DEFAULT_OOM_BACKEND          "auto"   /* auto | systemd-oomd | earlyoom | none */
#define DEFAULT_OOM_SWAP_THRESHOLD   90       /* % swap used before kill */
#define DEFAULT_OOM_MEM_THRESHOLD    90       /* % memory used before kill */
#define DEFAULT_OOM_PREFER_KILL_KTHREADD 0

/* dmesg-watch defaults */
#define DEFAULT_DMESG_SINCE_BOOT  1          /* 1 = scan from boot, 0 = only new */
#define DEFAULT_DMESG_SEVERITY    "warn"     /* emerg|alert|crit|err|warn */

/* failed-report defaults */
#define DEFAULT_REPORT_JOURNAL_LINES  30     /* lines of journal per failed unit */
#define DEFAULT_REPORT_OUTPUT         "stdout" /* stdout | file */
#define DEFAULT_REPORT_FILE           "/var/log/narchstab-report.log"

/* External binaries */
#define SYSTEMCTL_BIN    "/usr/bin/systemctl"
#define JOURNALCTL_BIN   "/usr/bin/journalctl"
#define DMESG_BIN        "/usr/bin/dmesg"
#define SYSCTL_BIN       "/usr/bin/sysctl"
#define CURL_BIN         "/usr/bin/curl"
#define NOTIFY_SEND_BIN  "/usr/bin/notify-send"
#define EARLYOOM_BIN     "/usr/bin/earlyoom"
#define SYSTEMD_OOMD_BIN "/usr/lib/systemd/systemd-oomd"

/* earlyoom systemd service name */
#define EARLYOOM_SERVICE  "earlyoom.service"
#define OOMD_SERVICE      "systemd-oomd.service"

/* oomd drop-in path */
#define OOMD_DROPIN_DIR   "/etc/systemd/oomd.conf.d"
#define OOMD_DROPIN_FILE  "/etc/systemd/oomd.conf.d/narchstab.conf"

/* earlyoom sysconfig */
#define EARLYOOM_CONF     "/etc/default/earlyoom"

/* Limits */
#define MAX_FAILED_UNITS  256
#define MAX_DMESG_LINES   4096
#define MAX_LINE          1024
