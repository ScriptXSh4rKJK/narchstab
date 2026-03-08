#include "dmesg.h"
#include "cfg.h"
#include "log.h"
#include "notify.h"

/* ---------------------------------------------------------------- severity -- */

typedef enum {
    DMSEV_EMERG = 0,
    DMSEV_ALERT,
    DMSEV_CRIT,
    DMSEV_ERR,
    DMSEV_WARN,
    DMSEV_NOTICE,
    DMSEV_INFO,
    DMSEV_DEBUG,
    DMSEV_UNKNOWN,
} DmesgSev;

static DmesgSev parse_dmesg_sev(const char *s) {
    if (!s) return DMSEV_UNKNOWN;
    if (strcasecmp(s, "emerg")  == 0) return DMSEV_EMERG;
    if (strcasecmp(s, "alert")  == 0) return DMSEV_ALERT;
    if (strcasecmp(s, "crit")   == 0) return DMSEV_CRIT;
    if (strcasecmp(s, "err")    == 0) return DMSEV_ERR;
    if (strcasecmp(s, "warn")   == 0) return DMSEV_WARN;
    if (strcasecmp(s, "notice") == 0) return DMSEV_NOTICE;
    if (strcasecmp(s, "info")   == 0) return DMSEV_INFO;
    if (strcasecmp(s, "debug")  == 0) return DMSEV_DEBUG;
    return DMSEV_UNKNOWN;
}

static const char *dmsev_label(DmesgSev s) {
    switch (s) {
    case DMSEV_EMERG:  return "emerg";
    case DMSEV_ALERT:  return "alert";
    case DMSEV_CRIT:   return "crit";
    case DMSEV_ERR:    return "err";
    case DMSEV_WARN:   return "warn";
    case DMSEV_NOTICE: return "notice";
    case DMSEV_INFO:   return "info";
    case DMSEV_DEBUG:  return "debug";
    default:           return "?";
    }
}

/* ----------------------------------------------------------- issue patterns -- */

typedef enum {
    CAT_MCE   = 0,   /* Machine Check Exception */
    CAT_IO,          /* I/O / block device error */
    CAT_MEM,         /* Memory / ECC */
    CAT_FS,          /* Filesystem error */
    CAT_NET,         /* Network hardware */
    CAT_USB,         /* USB disconnect / error */
    CAT_GPU,         /* GPU / DRM error */
    CAT_OOPS,        /* Kernel oops / BUG / panic */
    CAT_OTHER,
} IssueCategory;

typedef struct {
    const char   *pattern;
    IssueCategory cat;
    DmesgSev      min_sev;   /* report even if below cfg threshold */
} Pattern;

static const Pattern PATTERNS[] = {
    /* MCE */
    { "mce:",                       CAT_MCE,  DMSEV_ERR  },
    { "Machine check",              CAT_MCE,  DMSEV_ERR  },
    { "MCE",                        CAT_MCE,  DMSEV_WARN },
    /* I/O */
    { "I/O error",                  CAT_IO,   DMSEV_ERR  },
    { "end_request: I/O error",     CAT_IO,   DMSEV_ERR  },
    { "blk_update_request",         CAT_IO,   DMSEV_ERR  },
    { "ata",                        CAT_IO,   DMSEV_ERR  },
    { "SCSI error",                 CAT_IO,   DMSEV_ERR  },
    { "nvme",                       CAT_IO,   DMSEV_ERR  },
    { "medium error",               CAT_IO,   DMSEV_ERR  },
    { "Unhandled sense code",       CAT_IO,   DMSEV_WARN },
    /* Memory */
    { "ECC",                        CAT_MEM,  DMSEV_WARN },
    { "EDAC",                       CAT_MEM,  DMSEV_WARN },
    { "memory error",               CAT_MEM,  DMSEV_ERR  },
    { "Bad page",                   CAT_MEM,  DMSEV_ERR  },
    { "page allocation failure",    CAT_MEM,  DMSEV_WARN },
    { "out of memory",              CAT_MEM,  DMSEV_WARN },
    /* Filesystem */
    { "EXT4-fs error",              CAT_FS,   DMSEV_ERR  },
    { "BTRFS error",                CAT_FS,   DMSEV_ERR  },
    { "XFS",                        CAT_FS,   DMSEV_ERR  },
    { "filesystem error",           CAT_FS,   DMSEV_ERR  },
    { "journal commit I/O error",   CAT_FS,   DMSEV_ERR  },
    { "corrupt",                    CAT_FS,   DMSEV_ERR  },
    /* Network */
    { "eth",                        CAT_NET,  DMSEV_ERR  },
    { "link is down",               CAT_NET,  DMSEV_WARN },
    { "NIC Link is Down",           CAT_NET,  DMSEV_WARN },
    { "NETDEV WATCHDOG",            CAT_NET,  DMSEV_WARN },
    /* USB */
    { "USB disconnect",             CAT_USB,  DMSEV_WARN },
    { "usb reset",                  CAT_USB,  DMSEV_WARN },
    { "cannot reset",               CAT_USB,  DMSEV_WARN },
    /* GPU */
    { "drm",                        CAT_GPU,  DMSEV_ERR  },
    { "GPU HANG",                   CAT_GPU,  DMSEV_CRIT },
    { "amdgpu",                     CAT_GPU,  DMSEV_ERR  },
    { "nouveau",                    CAT_GPU,  DMSEV_ERR  },
    /* Kernel oops */
    { "kernel BUG",                 CAT_OOPS, DMSEV_CRIT },
    { "Oops",                       CAT_OOPS, DMSEV_CRIT },
    { "kernel panic",               CAT_OOPS, DMSEV_CRIT },
    { "BUG: unable to handle",      CAT_OOPS, DMSEV_CRIT },
    { "WARNING: CPU:",              CAT_OOPS, DMSEV_WARN },
    { "RIP:",                       CAT_OOPS, DMSEV_CRIT },
    { "Call Trace:",                CAT_OOPS, DMSEV_CRIT },
};

static const size_t NPATTERNS = sizeof(PATTERNS) / sizeof(PATTERNS[0]);

static const char *cat_label(IssueCategory c) {
    switch (c) {
    case CAT_MCE:   return "MCE (Machine Check)";
    case CAT_IO:    return "I/O / disk error";
    case CAT_MEM:   return "Memory error";
    case CAT_FS:    return "Filesystem error";
    case CAT_NET:   return "Network hardware";
    case CAT_USB:   return "USB error";
    case CAT_GPU:   return "GPU error";
    case CAT_OOPS:  return "Kernel Oops/BUG";
    default:        return "Other";
    }
}

/* ----------------------------------------------------------------- parsing -- */

typedef struct {
    char          text[MAX_LINE];
    DmesgSev      sev;
    IssueCategory cat;
    double        ts;          /* kernel timestamp in seconds */
} Issue;

/*
 * dmesg --level=emerg,alert,crit,err,warn --kernel --decode
 * outputs lines like:
 *   kern  :warn  : [  123.456789] some message
 * or with --human:
 *   [Mar15 10:23] ...
 * We use --decode --nopager to get machine-readable severity prefixes.
 */
static DmesgSev level_from_prefix(const char *line) {
    /* dmesg --decode gives "kern  :warn  : ..." */
    const char *colon2 = strchr(line, ':');
    if (!colon2) return DMSEV_UNKNOWN;
    colon2 = strchr(colon2 + 1, ':');
    if (!colon2) return DMSEV_UNKNOWN;

    /* Extract the level token between first and second colon */
    const char *start = strchr(line, ':');
    if (!start) return DMSEV_UNKNOWN;
    start++;
    while (*start == ' ') start++;
    char token[16] = {0};
    size_t i = 0;
    while (*start && *start != ':' && *start != ' ' && i < sizeof(token)-1)
        token[i++] = *start++;
    return parse_dmesg_sev(token);
}

static double ts_from_line(const char *line) {
    /* Look for [  123.456789] pattern */
    const char *ob = strchr(line, '[');
    if (!ob) return -1.0;
    double v = 0;
    if (sscanf(ob + 1, "%lf", &v) == 1) return v;
    return -1.0;
}

/*
 * Match a line against all patterns. Returns the first matching pattern
 * or NULL if no match.
 */
static const Pattern *match_patterns(const char *line) {
    for (size_t i = 0; i < NPATTERNS; i++) {
        if (strcasestr(line, PATTERNS[i].pattern) != NULL)
            return &PATTERNS[i];
    }
    return NULL;
}

static DmesgSev cfg_threshold(void) {
    return parse_dmesg_sev(g_cfg.dmesg_severity);
}

/* Build dmesg argv based on config */
static int build_dmesg_argv(char **argv, int max_args) {
    int i = 0;
    argv[i++] = DMESG_BIN;
    argv[i++] = "--decode";
    argv[i++] = "--nopager";
    argv[i++] = "--kernel";
    argv[i++] = "--level=emerg,alert,crit,err,warn,notice";
    /* --since-boot is not a valid dmesg flag; dmesg always shows the current
     * boot's ring buffer by default. When dmesg_since_boot=no we would need
     * a persistent cursor — for now simply always read the full ring buffer
     * (the correct and portable behaviour). */
    argv[i] = NULL;
    if (i >= max_args - 1) {
        LOGE("build_dmesg_argv: argv overflow");
        return -1;
    }
    return i;
}

/* ----------------------------------------------------------------- report -- */

static void print_separator(void) {
    printf("  %-10s %-20s %s\n",
           "Severity", "Category", "Message");
    printf("  %s\n",
           "--------------------------------------------------------------"
           "----------------------");
}

int dmesg_run(void) {
    if (access(DMESG_BIN, X_OK) != 0) {
        LOGE("dmesg not found at %s", DMESG_BIN);
        return -1;
    }

    /* Capture dmesg output into a temp file to avoid huge in-memory buffers */
    char tmppath[] = "/tmp/narchstab_dmesg_XXXXXX";
    int tmpfd = mkstemp(tmppath);
    if (tmpfd < 0) {
        LOGE("mkstemp: %s", strerror(errno));
        return -1;
    }
    /* mkstemp creates with 0600, but be explicit */
    (void)fchmod(tmpfd, 0600);
    close(tmpfd);

    char *argv[16];
    build_dmesg_argv(argv, 16);

    if (proc_run_to_file(DMESG_BIN, argv, tmppath) < 0) {
        unlink(tmppath);
        return -1;
    }

    FILE *f = fopen(tmppath, "r");
    if (!f) {
        LOGE("Cannot read dmesg output: %s", strerror(errno));
        unlink(tmppath);
        return -1;
    }

    DmesgSev threshold = cfg_threshold();
    if (threshold == DMSEV_UNKNOWN) threshold = DMSEV_WARN;

    /* Collect issues */
    Issue *issues = calloc((size_t)MAX_DMESG_LINES, sizeof(Issue));
    if (!issues) { fclose(f); unlink(tmppath); return -1; }
    int nissues = 0;

    char line[MAX_LINE];
    while (fgets(line, (int)sizeof(line), f) && nissues < MAX_DMESG_LINES) {
        /* Strip newline */
        char *nl = strchr(line, '\n'); if (nl) *nl = '\0';

        DmesgSev line_sev = level_from_prefix(line);
        const Pattern *pat = match_patterns(line);

        /* Include line if it matches a pattern OR is above severity threshold */
        int include = 0;
        if (pat && pat->min_sev <= threshold) include = 1;
        if (line_sev != DMSEV_UNKNOWN && line_sev <= threshold) include = 1;

        if (!include) continue;

        Issue *iss = &issues[nissues++];
        snprintf(iss->text, sizeof(iss->text), "%s", line);
        iss->sev = (line_sev != DMSEV_UNKNOWN) ? line_sev
                   : (pat ? pat->min_sev : DMSEV_WARN);
        iss->cat = pat ? pat->cat : CAT_OTHER;
        iss->ts  = ts_from_line(line);
    }
    fclose(f);
    unlink(tmppath);

    /* Header */
    printf("\n==========================================\n");
    printf("  dmesg-watch — hardware error scan\n");
    printf("==========================================\n");
    printf("  Severity threshold : %s\n", g_cfg.dmesg_severity);
    printf("  Since boot         : %s\n", g_cfg.dmesg_since_boot ? "yes" : "no");
    printf("  Issues found       : %d\n\n", nissues);

    if (nissues == 0) {
        printf("  No hardware issues detected. System looks clean.\n");
        LOGI("dmesg-watch: no issues found");
        free(issues);
        return 0;
    }

    /* Array indexed by IssueCategory; CAT_OTHER is the last value */
    int cat_counts[CAT_OTHER + 1];
    memset(cat_counts, 0, sizeof(cat_counts));
    for (int i = 0; i < nissues; i++) {
        int c = (int)issues[i].cat;
        if (c >= 0 && c <= (int)CAT_OTHER)
            cat_counts[c]++;
    }

    printf("  Summary by category:\n");
    for (int c = 0; c <= (int)CAT_OTHER; c++) {
        if (cat_counts[c] > 0)
            printf("    %-24s : %d\n", cat_label((IssueCategory)c), cat_counts[c]);
    }
    printf("\n  Detail:\n");
    print_separator();

    /* Count severe issues for notification */
    int ncrit = 0, nwarn = 0;
    char notify_body[1024] = {0};
    size_t notify_pos = 0;

    for (int i = 0; i < nissues; i++) {
        Issue *iss = &issues[i];
        /* Find the message part after the timestamp */
        const char *msg = iss->text;
        const char *cb = strchr(msg, ']');
        if (cb) msg = cb + 1;
        while (*msg == ' ') msg++;

        printf("  %-10s %-20s %s\n",
               dmsev_label(iss->sev), cat_label(iss->cat), msg);

        if (iss->sev <= DMSEV_CRIT) ncrit++;
        else if (iss->sev <= DMSEV_WARN) nwarn++;

        /* Collect first few lines into notification body */
        if (notify_pos + 2 < sizeof(notify_body)) {
            int written = snprintf(notify_body + notify_pos,
                                   sizeof(notify_body) - notify_pos,
                                   "[%s] %s\n",
                                   dmsev_label(iss->sev), msg);
            if (written > 0 && (size_t)written < sizeof(notify_body) - notify_pos)
                notify_pos += (size_t)written;
        }
    }

    printf("\n");
    LOGW("dmesg-watch: %d critical, %d warning issues found", ncrit, nwarn);

    /* Send notification */
    if (ncrit > 0) {
        notify_send(SEV_CRIT, "dmesg-watch: CRITICAL hardware errors",
                    "%d critical / %d warning issues\n%s",
                    ncrit, nwarn, notify_body);
    } else if (nwarn > 0) {
        notify_send(SEV_WARN, "dmesg-watch: hardware warnings",
                    "%d warning issues\n%s", nwarn, notify_body);
    }

    free(issues);
    return nissues;
}
