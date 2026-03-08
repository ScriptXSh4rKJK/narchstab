#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <dirent.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <sys/file.h>
#include <syslog.h>

#include "config.h"

typedef enum {
    EXIT_OK          = 0,
    EXIT_ERR_GENERIC = 1,
    EXIT_ERR_PERM    = 2,
    EXIT_ERR_LOCK    = 3,
    EXIT_ERR_CONFIG  = 4,
} ExitCode;

/* Global state */
extern FILE *g_logfile;
extern int   g_dry_run;
extern int   g_verbose;
extern int   g_lock_fd;

/* Convenience: run a child process, return exit code */
int  proc_run(const char *path, char *const argv[]);
int  proc_run_capture(const char *path, char *const argv[],
                      char *buf, size_t bufsz);
int  proc_run_to_file(const char *path, char *const argv[],
                      const char *outpath);
/* Like proc_run but honours g_dry_run */
int  proc_run_mut(const char *path, char *const argv[]);

/* Lock file */
int  lock_acquire(const char *path);
void lock_release(int fd, const char *path);
