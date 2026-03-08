#include "common.h"
#include "cfg.h"
#include "log.h"
#include "notify.h"
#include "oom.h"
#include "dmesg.h"
#include "report.h"

static void print_usage(const char *argv0) {
    printf(
        "Usage:\n"
        "  %s oom              Configure OOM killer (systemd-oomd or earlyoom)\n"
        "  %s dmesg            Scan dmesg for hardware errors\n"
        "  %s report           Report failed systemd units with journal output\n"
        "\n"
        "Options:\n"
        "  --config <FILE>     Config file path (default: %s)\n"
        "  --dry-run           Simulate changes without applying them\n"
        "  --verbose           More output\n"
        "  --show-config       Print active configuration and exit\n"
        "  -h, --help          Show this help\n"
        "  -v, --version       Print version\n"
        "\n"
        "Config file: %s\n"
        "Log file:    (see log_file in config)\n"
        "\n"
        "Integration with narchsafe (optional):\n"
        "  Set narchsafe_conf = /etc/narchsafe.conf in %s\n"
        "  to inherit Telegram credentials. All other settings remain independent.\n",
        argv0, argv0, argv0,
        DEFAULT_CONF_FILE, DEFAULT_CONF_FILE, DEFAULT_CONF_FILE);
}

int main(int argc, char *argv[]) {
    cfg_load_defaults();

    /* Pre-scan for --config before log_init */
    const char *conf_path = DEFAULT_CONF_FILE;
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--config") == 0) {
            conf_path = argv[i + 1];
            break;
        }
    }

    cfg_parse_file(conf_path);
    cfg_inherit_narchsafe();

    log_init();

    /* Parse remaining arguments */
    const char *subcmd     = NULL;
    int         show_cfg   = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]); return EXIT_OK;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("%s v%s\n", PROG_NAME, NS_VERSION); return EXIT_OK;
        }
        if (strcmp(argv[i], "--dry-run")     == 0) { g_dry_run  = 1; continue; }
        if (strcmp(argv[i], "--verbose")     == 0) { g_verbose  = 1; continue; }
        if (strcmp(argv[i], "--show-config") == 0) { show_cfg   = 1; continue; }
        if (strcmp(argv[i], "--config")      == 0) { i++;             continue; }

        /* Subcommands */
        if (strcmp(argv[i], "oom")    == 0 ||
            strcmp(argv[i], "dmesg")  == 0 ||
            strcmp(argv[i], "report") == 0) {
            subcmd = argv[i];
            continue;
        }

        fprintf(stderr, "Unknown argument: %s\n\n", argv[i]);
        print_usage(argv[0]);
        return EXIT_ERR_GENERIC;
    }

    if (show_cfg) { cfg_print(); return EXIT_OK; }

    if (!subcmd) {
        fprintf(stderr, "No subcommand given.\n\n");
        print_usage(argv[0]);
        return EXIT_ERR_GENERIC;
    }

    /* Acquire lock to prevent parallel runs of the same subcommand */
    g_lock_fd = lock_acquire(g_cfg.lock_file);
    if (g_lock_fd < 0) return EXIT_ERR_LOCK;

    LOGI("=== %s v%s  subcmd=%s dry_run=%d ===",
         PROG_NAME, NS_VERSION, subcmd, g_dry_run);

    int rc = EXIT_ERR_GENERIC;

    if (strcmp(subcmd, "oom") == 0) {
        rc = oom_run();
        if (rc == 0)
            printf("\n[oom-policy] Done.\n");
        else
            fprintf(stderr, "\n[oom-policy] Finished with errors.\n");

    } else if (strcmp(subcmd, "dmesg") == 0) {
        int found = dmesg_run();
        if (found < 0) {
            fprintf(stderr, "\n[dmesg-watch] Error during scan.\n");
            rc = EXIT_ERR_GENERIC;
        } else {
            rc = EXIT_OK;
        }

    } else if (strcmp(subcmd, "report") == 0) {
        int failed = report_run();
        if (failed < 0) {
            fprintf(stderr, "\n[failed-report] Error generating report.\n");
            rc = EXIT_ERR_GENERIC;
        } else {
            if (failed == 0) printf("\n[failed-report] No failed units. System is healthy.\n");
            rc = EXIT_OK;
        }
    }

    lock_release(g_lock_fd, g_cfg.lock_file);
    g_lock_fd = -1;
    log_close();
    return rc;
}
