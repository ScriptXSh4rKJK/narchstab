#include "cfg.h"
#include "log.h"

NarchstabConfig g_cfg;

static void trim(char *s) {
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\r' || s[len-1] == '\n'))
        s[--len] = '\0';
}

static void strip_comment(char *s) {
    int in_val = 0;
    for (char *p = s; *p; p++) {
        if (!in_val && (*p == ' ' || *p == '\t')) continue;
        in_val = 1;
        if (*p == '#' || *p == ';') { *p = '\0'; break; }
    }
    trim(s);
}

static int parse_bool(const char *s) {
    if (!s) return -1;
    if (strcmp(s,"1")==0 || strcasecmp(s,"yes")==0  ||
        strcasecmp(s,"true")==0  || strcasecmp(s,"on")==0)  return 1;
    if (strcmp(s,"0")==0 || strcasecmp(s,"no")==0   ||
        strcasecmp(s,"false")==0 || strcasecmp(s,"off")==0) return 0;
    return -1;
}

void cfg_load_defaults(void) {
    memset(&g_cfg, 0, sizeof(g_cfg));

    snprintf(g_cfg.log_file,         sizeof(g_cfg.log_file),         "%s", DEFAULT_LOG_FILE);
    snprintf(g_cfg.lock_file,        sizeof(g_cfg.lock_file),        "%s", DEFAULT_LOCK_FILE);
    snprintf(g_cfg.state_dir,        sizeof(g_cfg.state_dir),        "%s", DEFAULT_STATE_DIR);
    snprintf(g_cfg.telegram_api_url, sizeof(g_cfg.telegram_api_url), "%s", DEFAULT_TELEGRAM_API_URL);
    snprintf(g_cfg.oom_backend,      sizeof(g_cfg.oom_backend),      "%s", DEFAULT_OOM_BACKEND);
    snprintf(g_cfg.dmesg_severity,   sizeof(g_cfg.dmesg_severity),   "%s", DEFAULT_DMESG_SEVERITY);
    snprintf(g_cfg.report_output,    sizeof(g_cfg.report_output),    "%s", DEFAULT_REPORT_OUTPUT);
    snprintf(g_cfg.report_file,      sizeof(g_cfg.report_file),      "%s", DEFAULT_REPORT_FILE);

    g_cfg.notify_log           = 1;
    g_cfg.notify_telegram      = 0;
    g_cfg.notify_libnotify     = 0;
    g_cfg.oom_mem_threshold    = DEFAULT_OOM_MEM_THRESHOLD;
    g_cfg.oom_swap_threshold   = DEFAULT_OOM_SWAP_THRESHOLD;
    g_cfg.dmesg_since_boot     = DEFAULT_DMESG_SINCE_BOOT;
    g_cfg.report_journal_lines = DEFAULT_REPORT_JOURNAL_LINES;
}

static int parse_one(const char *path, int lineno,
                     const char *key, char *val) {
#define STR(field, cfgkey) \
    if (strcmp(key, cfgkey) == 0) { \
        snprintf(g_cfg.field, sizeof(g_cfg.field), "%s", val); return 0; \
    }
#define INT_POS(field, cfgkey) \
    if (strcmp(key, cfgkey) == 0) { \
        int v = atoi(val); \
        if (v > 0) { g_cfg.field = v; } \
        else { LOGW("%s:%d: %s must be > 0", path, lineno, cfgkey); } \
        return 0; \
    }
#define INT_PCT(field, cfgkey) \
    if (strcmp(key, cfgkey) == 0) { \
        int v = atoi(val); \
        if (v > 0 && v <= 100) { g_cfg.field = v; } \
        else { LOGW("%s:%d: %s must be 1-100", path, lineno, cfgkey); } \
        return 0; \
    }
#define BOOL(field, cfgkey) \
    if (strcmp(key, cfgkey) == 0) { \
        int b = parse_bool(val); \
        if (b >= 0) { g_cfg.field = b; } \
        else { LOGW("%s:%d: invalid bool '%s' for %s", path, lineno, val, cfgkey); } \
        return 0; \
    }

    /* General */
    STR(log_file,        "log_file")
    STR(lock_file,       "lock_file")
    STR(state_dir,       "state_dir")
    STR(narchsafe_conf,  "narchsafe_conf")

    /* Notifications */
    BOOL(notify_telegram,  "notify_telegram")
    BOOL(notify_libnotify, "notify_libnotify")
    BOOL(notify_log,       "notify_log")
    STR(telegram_token,    "telegram_token")
    STR(telegram_chat_id,  "telegram_chat_id")
    STR(telegram_api_url,  "telegram_api_url")

    /* OOM */
    STR(oom_backend,       "oom_backend")
    INT_PCT(oom_mem_threshold,  "oom_mem_threshold")
    INT_PCT(oom_swap_threshold, "oom_swap_threshold")

    /* dmesg */
    BOOL(dmesg_since_boot, "dmesg_since_boot")
    STR(dmesg_severity,    "dmesg_severity")

    /* report */
    INT_POS(report_journal_lines, "report_journal_lines")
    STR(report_output,     "report_output")
    STR(report_file,       "report_file")

    LOGW("%s:%d: unknown key '%s'", path, lineno, key);
    return 0;

#undef STR
#undef INT_POS
#undef INT_PCT
#undef BOOL
}

int cfg_parse_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        LOGI("No config at %s, using defaults", path);
        return -1;
    }
    LOGI("Loading config: %s", path);
    char line[512];
    int  lineno = 0;
    while (fgets(line, (int)sizeof(line), f)) {
        lineno++;
        trim(line);
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') continue;
        char *eq = strchr(line, '=');
        if (!eq) { LOGW("%s:%d: missing '='", path, lineno); continue; }
        *eq = '\0';
        char *key = line, *val = eq + 1;
        trim(key); trim(val); strip_comment(val);
        parse_one(path, lineno, key, val);
    }
    fclose(f);
    return 0;
}

/*
 * If narchsafe_conf is set and telegram creds are not yet configured,
 * try to read them from the narchsafe config file.
 */
void cfg_inherit_narchsafe(void) {
    if (g_cfg.narchsafe_conf[0] == '\0') return;
    /* Only inherit if BOTH creds are missing — partial config means user set them intentionally */
    if (g_cfg.telegram_token[0] != '\0' && g_cfg.telegram_chat_id[0] != '\0') return;

    FILE *f = fopen(g_cfg.narchsafe_conf, "r");
    if (!f) {
        LOGW("narchsafe_conf set but cannot open: %s", g_cfg.narchsafe_conf);
        return;
    }
    LOGI("Inheriting Telegram creds from: %s", g_cfg.narchsafe_conf);
    char line[512];
    int lineno = 0;
    while (fgets(line, (int)sizeof(line), f)) {
        lineno++;
        trim(line);
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line, *val = eq + 1;
        trim(key); trim(val); strip_comment(val);
        if (strcmp(key, "telegram_token")   == 0)
            snprintf(g_cfg.telegram_token,   sizeof(g_cfg.telegram_token),   "%s", val);
        if (strcmp(key, "telegram_chat_id") == 0)
            snprintf(g_cfg.telegram_chat_id, sizeof(g_cfg.telegram_chat_id), "%s", val);
        if (strcmp(key, "telegram_api_url") == 0)
            snprintf(g_cfg.telegram_api_url, sizeof(g_cfg.telegram_api_url), "%s", val);
    }
    fclose(f);
}

void cfg_print(void) {
    printf("\n-- Configuration (%s v%s) --\n", PROG_NAME, NS_VERSION);
    printf("  log_file           = %s\n", g_cfg.log_file);
    printf("  lock_file          = %s\n", g_cfg.lock_file);
    printf("  state_dir          = %s\n", g_cfg.state_dir);
    printf("  narchsafe_conf     = %s\n",
           g_cfg.narchsafe_conf[0] ? g_cfg.narchsafe_conf : "(not set — standalone mode)");
    printf("  notify_log         = %s\n", g_cfg.notify_log      ? "yes" : "no");
    printf("  notify_libnotify   = %s\n", g_cfg.notify_libnotify ? "yes" : "no");
    printf("  notify_telegram    = %s\n", g_cfg.notify_telegram  ? "yes" : "no");
    if (g_cfg.notify_telegram) {
        printf("  telegram_api_url   = %s\n", g_cfg.telegram_api_url);
        printf("  telegram_chat_id   = %s\n",
               g_cfg.telegram_chat_id[0] ? g_cfg.telegram_chat_id : "(not set)");
        printf("  telegram_token     = %s\n",
               g_cfg.telegram_token[0]   ? "***"                  : "(not set)");
    }
    printf("  oom_backend        = %s\n", g_cfg.oom_backend);
    printf("  oom_mem_threshold  = %d%%\n",  g_cfg.oom_mem_threshold);
    printf("  oom_swap_threshold = %d%%\n",  g_cfg.oom_swap_threshold);
    printf("  dmesg_since_boot   = %s\n", g_cfg.dmesg_since_boot ? "yes" : "no");
    printf("  dmesg_severity     = %s\n", g_cfg.dmesg_severity);
    printf("  report_journal_lines = %d\n", g_cfg.report_journal_lines);
    printf("  report_output      = %s\n", g_cfg.report_output);
    if (strcmp(g_cfg.report_output, "file") == 0)
        printf("  report_file        = %s\n", g_cfg.report_file);
    printf("\n");
}
