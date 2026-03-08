#include "report.h"
#include "cfg.h"
#include "log.h"
#include "notify.h"

typedef struct {
    char unit[256];
    char load[32];
    char active[32];
    char sub[32];
    char description[512];
} FailedUnit;

/* ---------------------------------------------------------- collect units -- */

/*
 * Run: systemctl list-units --state=failed --no-legend --no-pager
 * Output format (space-separated):
 *   UNIT   LOAD   ACTIVE  SUB   DESCRIPTION...
 */
static int collect_failed(FailedUnit *arr, int max) {
    if (access(SYSTEMCTL_BIN, X_OK) != 0) {
        LOGE("systemctl not found at %s", SYSTEMCTL_BIN);
        return -1;
    }

    char tmppath[] = "/tmp/narchstab_units_XXXXXX";
    int tmpfd = mkstemp(tmppath);
    if (tmpfd < 0) { LOGE("mkstemp: %s", strerror(errno)); return -1; }
    (void)fchmod(tmpfd, 0600);
    close(tmpfd);

    char *argv[] = {
        SYSTEMCTL_BIN, "list-units",
        "--state=failed",
        "--no-legend",
        "--no-pager",
        "--plain",
        NULL
    };
    int rc = proc_run_to_file(SYSTEMCTL_BIN, argv, tmppath);
    if (rc != 0)
        LOGW("systemctl list-units exited with %d (may be no failed units)", rc);

    FILE *f = fopen(tmppath, "r");
    if (!f) { unlink(tmppath); return 0; }

    int count = 0;
    char line[MAX_LINE];
    while (fgets(line, (int)sizeof(line), f) && count < max) {
        char *nl = strchr(line, '\n'); if (nl) *nl = '\0';
        if (line[0] == '\0') continue;

        FailedUnit *u = &arr[count];
        /* sscanf: first 4 fixed fields, rest is description */
        int n = sscanf(line, "%255s %31s %31s %31s",
                       u->unit, u->load, u->active, u->sub);
        if (n < 1) continue;

        /* Description: everything after the 4th field */
        const char *p = line;
        int fields = 0;
        while (*p) {
            while (*p == ' ') p++;
            if (*p == '\0') break;
            fields++;
            while (*p && *p != ' ') p++;
            if (fields == 4) { while (*p == ' ') p++; break; }
        }
        size_t plen = strnlen(p, sizeof(u->description) - 1);
        memcpy(u->description, p, plen);
        u->description[plen] = '\0';
        count++;
    }
    fclose(f);
    unlink(tmppath);
    return count;
}

/* -------------------------------------------------------- journal snippet -- */

static void print_journal(FILE *out, const char *unit, int nlines) {
    if (access(JOURNALCTL_BIN, X_OK) != 0) return;

    char tmppath[] = "/tmp/narchstab_journal_XXXXXX";
    int tmpfd = mkstemp(tmppath);
    if (tmpfd < 0) return;
    (void)fchmod(tmpfd, 0600);
    close(tmpfd);

    char nlines_str[16];
    snprintf(nlines_str, sizeof(nlines_str), "%d", nlines);

    char *argv[] = {
        JOURNALCTL_BIN,
        "-u", (char *)unit,
        "-n", nlines_str,
        "--no-pager",
        "--output=short-monotonic",
        NULL
    };
    proc_run_to_file(JOURNALCTL_BIN, argv, tmppath);

    FILE *jf = fopen(tmppath, "r");
    if (!jf) { unlink(tmppath); return; }

    char line[MAX_LINE];
    while (fgets(line, (int)sizeof(line), jf))
        fprintf(out, "    %s", line);
    fclose(jf);
    unlink(tmppath);
}

/* --------------------------------------------------- last activation time -- */

static void safe_copy(char *dst, size_t dstsz, const char *src) {
    if (!src || src[0] == '\0') {
        memcpy(dst, "n/a", 4);
        return;
    }
    size_t len = strnlen(src, dstsz - 1);
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void get_unit_timestamps(const char *unit,
                                char *active_ts, size_t tsz,
                                char *inactive_ts, size_t itsz) {
    active_ts[0] = '\0';
    inactive_ts[0] = '\0';

    char prop[512];
    {
        char *argv[] = {
            SYSTEMCTL_BIN, "show", (char *)unit,
            "--property=ActiveEnterTimestamp",
            "--value", "--no-pager", NULL
        };
        proc_run_capture(SYSTEMCTL_BIN, argv, prop, sizeof(prop));
        safe_copy(active_ts, tsz, prop);
    }
    {
        char *argv[] = {
            SYSTEMCTL_BIN, "show", (char *)unit,
            "--property=InactiveEnterTimestamp",
            "--value", "--no-pager", NULL
        };
        proc_run_capture(SYSTEMCTL_BIN, argv, prop, sizeof(prop));
        safe_copy(inactive_ts, itsz, prop);
    }
}

static void get_unit_exit_code(const char *unit, char *buf, size_t sz) {
    char *argv[] = {
        SYSTEMCTL_BIN, "show", (char *)unit,
        "--property=ExecMainStatus",
        "--value", "--no-pager", NULL
    };
    proc_run_capture(SYSTEMCTL_BIN, argv, buf, sz);
}

/* --------------------------------------------------------- write report -- */

static void write_report(FILE *out, FailedUnit *units, int n) {
    time_t now = time(NULL);
    struct tm tmbuf;
    char ts[64] = "unknown";
    if (localtime_r(&now, &tmbuf))
        strftime(ts, sizeof(ts), "%F %T", &tmbuf);

    fprintf(out,
        "==========================================\n"
        "  failed-units-report — %s v%s\n"
        "  Generated: %s\n"
        "==========================================\n\n",
        PROG_NAME, NS_VERSION, ts);

    if (n == 0) {
        fprintf(out, "  No failed units. System is clean.\n\n");
        return;
    }

    fprintf(out, "  Failed units: %d\n\n", n);

    for (int i = 0; i < n; i++) {
        FailedUnit *u = &units[i];

        char active_ts[128], inactive_ts[128], exit_code[32];
        get_unit_timestamps(u->unit, active_ts, sizeof(active_ts),
                                     inactive_ts, sizeof(inactive_ts));
        get_unit_exit_code(u->unit, exit_code, sizeof(exit_code));

        fprintf(out,
            "------------------------------------------\n"
            "  [%d/%d] %s\n"
            "------------------------------------------\n"
            "  Description  : %s\n"
            "  Load         : %s\n"
            "  Active       : %s (%s)\n"
            "  Last started : %s\n"
            "  Failed at    : %s\n"
            "  Exit code    : %s\n\n"
            "  Recent journal:\n",
            i + 1, n,
            u->unit,
            u->description[0] ? u->description : "(no description)",
            u->load,
            u->active, u->sub,
            active_ts[0]   ? active_ts   : "n/a",
            inactive_ts[0] ? inactive_ts : "n/a",
            exit_code[0]   ? exit_code   : "n/a");

        print_journal(out, u->unit, g_cfg.report_journal_lines);

        fprintf(out,
            "\n  Suggested fix:\n"
            "    systemctl status %s\n"
            "    journalctl -xe -u %s\n"
            "    systemctl restart %s\n\n",
            u->unit, u->unit, u->unit);
    }

    fprintf(out,
        "==========================================\n"
        "  To reset all failed units:\n"
        "    systemctl reset-failed\n"
        "  Log: %s\n"
        "==========================================\n",
        g_cfg.log_file);
}

/* -------------------------------------------------------------- public API -- */

int report_run(void) {
    FailedUnit *units = calloc((size_t)MAX_FAILED_UNITS, sizeof(FailedUnit));
    if (!units) { LOGE("Out of memory"); return -1; }

    int n = collect_failed(units, MAX_FAILED_UNITS);
    if (n < 0) { free(units); return -1; }

    /* Determine output stream */
    FILE *out = stdout;
    int   close_out = 0;

    if (strcmp(g_cfg.report_output, "file") == 0) {
        int fd = open(g_cfg.report_file,
                      O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0640);
        if (fd < 0) {
            LOGE("Cannot open report file %s: %s",
                 g_cfg.report_file, strerror(errno));
        } else {
            out = fdopen(fd, "w");
            if (!out) { close(fd); out = stdout; }
            else       close_out = 1;
        }
    }

    write_report(out, units, n);
    if (close_out) {
        fclose(out);
        printf("  Report written to: %s\n", g_cfg.report_file);
        LOGI("failed-units-report: %d units, report: %s", n, g_cfg.report_file);
    } else {
        LOGI("failed-units-report: %d units", n);
    }

    /* Notify if any units are failed */
    if (n > 0) {
        char body[512] = {0};
        size_t pos = 0;
        for (int i = 0; i < n && pos < sizeof(body) - 64; i++) {
            int w = snprintf(body + pos, sizeof(body) - pos,
                             "• %s\n", units[i].unit);
            if (w > 0) pos += (size_t)w;
        }
        notify_send(SEV_WARN, "failed-units-report",
                    "%d failed unit(s):\n%s", n, body);
    }

    free(units);
    return n;
}
