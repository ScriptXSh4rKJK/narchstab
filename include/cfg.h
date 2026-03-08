#pragma once
#include "common.h"

typedef struct {
    /* General */
    char log_file[PATH_MAX];
    char lock_file[PATH_MAX];
    char state_dir[PATH_MAX];

    /* Notifications */
    int  notify_telegram;
    char telegram_token[256];
    char telegram_chat_id[64];
    char telegram_api_url[256];

    int  notify_libnotify;
    int  notify_log;         /* always-on, but can suppress */

    /* Optional integration: path to narchsafe config to inherit
     * telegram creds from. Leave empty to disable. */
    char narchsafe_conf[PATH_MAX];

    /* OOM policy */
    char oom_backend[32];    /* auto | systemd-oomd | earlyoom | none */
    int  oom_mem_threshold;  /* percent */
    int  oom_swap_threshold; /* percent */

    /* dmesg-watch */
    int  dmesg_since_boot;
    char dmesg_severity[16]; /* emerg|alert|crit|err|warn */

    /* failed-report */
    int  report_journal_lines;
    char report_output[16];  /* stdout | file */
    char report_file[PATH_MAX];
} NarchstabConfig;

extern NarchstabConfig g_cfg;

void cfg_load_defaults(void);
int  cfg_parse_file(const char *path);
/* Optionally inherit telegram creds from a narchsafe.conf */
void cfg_inherit_narchsafe(void);
void cfg_print(void);
