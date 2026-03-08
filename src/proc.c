#include "common.h"
#include "log.h"

FILE *g_logfile = NULL;
int   g_dry_run = 0;
int   g_verbose = 0;
int   g_lock_fd = -1;

static char *const SAFE_ENV[] = {
    "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
    "HOME=/root",
    "LANG=C",
    NULL
};

static void child_init(void) {
    if (g_logfile) { fclose(g_logfile); g_logfile = NULL; }
    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, NULL);
    signal(SIGINT,  SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
}

static int wait_child(pid_t pid, const char *name) {
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        LOGE("waitpid(%s): %s", name, strerror(errno));
        return -1;
    }
    if (WIFSIGNALED(status)) {
        LOGW("%s killed by signal %d", name, WTERMSIG(status));
        return -1;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

int proc_run(const char *path, char *const argv[]) {
    if (access(path, X_OK) != 0) {
        LOGE("%s: not found or not executable", path);
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) { LOGE("fork: %s", strerror(errno)); return -1; }
    if (pid == 0) {
        child_init();
        execve(path, argv, SAFE_ENV);
        _exit(127);
    }
    return wait_child(pid, path);
}

int proc_run_capture(const char *path, char *const argv[],
                     char *buf, size_t bufsz) {
    if (!buf || bufsz == 0) return -1;
    buf[0] = '\0';
    if (access(path, X_OK) != 0) { LOGE("%s: not found", path); return -1; }

    int pfd[2];
    if (pipe2(pfd, O_CLOEXEC) != 0) { LOGE("pipe2: %s", strerror(errno)); return -1; }

    pid_t pid = fork();
    if (pid < 0) {
        close(pfd[0]); close(pfd[1]);
        LOGE("fork: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        /* dup2 clears O_CLOEXEC on the new fd — that is intentional here */
        if (dup2(pfd[1], STDOUT_FILENO) < 0) _exit(1);
        int devnull = open("/dev/null", O_WRONLY | O_CLOEXEC);
        if (devnull >= 0) dup2(devnull, STDERR_FILENO);
        child_init();   /* closes pfd[0] and pfd[1] via O_CLOEXEC on exec */
        execve(path, argv, SAFE_ENV);
        _exit(127);
    }
    close(pfd[1]);

    size_t total = 0;
    ssize_t n;
    while (total < bufsz - 1 &&
           (n = read(pfd[0], buf + total, bufsz - 1 - total)) > 0)
        total += (size_t)n;
    buf[total] = '\0';
    if (total > 0 && buf[total - 1] == '\n') buf[--total] = '\0';
    close(pfd[0]);
    return wait_child(pid, path);
}

int proc_run_to_file(const char *path, char *const argv[],
                     const char *outpath) {
    if (access(path, X_OK) != 0) { LOGE("%s: not found", path); return -1; }
    pid_t pid = fork();
    if (pid < 0) { LOGE("fork: %s", strerror(errno)); return -1; }
    if (pid == 0) {
        int fd = open(outpath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
        if (fd < 0) _exit(1);
        if (dup2(fd, STDOUT_FILENO) < 0) _exit(1);
        /* Redirect stderr to /dev/null to avoid polluting the terminal */
        int devnull = open("/dev/null", O_WRONLY | O_CLOEXEC);
        if (devnull >= 0) dup2(devnull, STDERR_FILENO);
        child_init();  /* called after dup2; closes fd/devnull via O_CLOEXEC */
        execve(path, argv, SAFE_ENV);
        _exit(127);
    }
    return wait_child(pid, path);
}

int proc_run_mut(const char *path, char *const argv[]) {
    if (g_dry_run) {
        printf("  [DRY-RUN] would run:");
        for (int i = 0; argv[i]; i++) printf(" %s", argv[i]);
        printf("\n");
        return 0;
    }
    return proc_run(path, argv);
}

/* ------------------------------------------------------------------ lock -- */

int lock_acquire(const char *path) {
    int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) {
        LOGE("Cannot open lock file %s: %s", path, strerror(errno));
        return -1;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        if (errno == EWOULDBLOCK) {
            /* Read PID while we still hold the fd — still a best-effort
             * display, but safe because we opened the file before the
             * contention check and the holder keeps it open. */
            char pidbuf[32] = {0};
            ssize_t r = read(fd, pidbuf, sizeof(pidbuf) - 1);
            if (r > 0) {
                pidbuf[r] = '\0';
                /* strip newline */
                char *nl = strchr(pidbuf, '\n'); if (nl) *nl = '\0';
                LOGE("Another instance is running (PID %s)", pidbuf);
            } else {
                LOGE("Another instance is already running");
            }
        } else {
            LOGE("flock: %s", strerror(errno));
        }
        close(fd);
        return -1;
    }
    if (ftruncate(fd, 0) != 0) {
        LOGE("ftruncate: %s", strerror(errno));
        close(fd);
        return -1;
    }
    char pidbuf[32];
    int n = snprintf(pidbuf, sizeof(pidbuf), "%lld\n", (long long)getpid());
    if (n > 0) {
        if (write(fd, pidbuf, (size_t)n) != (ssize_t)n)
            LOGW("Failed to write PID to lock file");
        else
            (void)fsync(fd);   /* ensure PID is visible to other instances */
    }
    return fd;
}

void lock_release(int fd, const char *path) {
    if (fd < 0) return;
    flock(fd, LOCK_UN);
    close(fd);
    unlink(path);
}
