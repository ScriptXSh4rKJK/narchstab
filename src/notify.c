#include "notify.h"
#include "cfg.h"
#include "log.h"

static const char *sev_icon(Severity sev) {
    switch (sev) {
    case SEV_CRIT: return "dialog-error";
    case SEV_WARN: return "dialog-warning";
    default:       return "dialog-information";
    }
}

static const char *sev_str(Severity sev) {
    switch (sev) {
    case SEV_CRIT: return "CRITICAL";
    case SEV_WARN: return "WARNING";
    default:       return "INFO";
    }
}

static void notify_libnotify(const char *title, const char *body, Severity sev) {
    if (access(NOTIFY_SEND_BIN, X_OK) != 0) {
        LOGW("notify-send not found at %s", NOTIFY_SEND_BIN);
        return;
    }
    const char *urgency = (sev == SEV_CRIT) ? "critical" :
                          (sev == SEV_WARN) ? "normal"   : "low";
    pid_t pid = fork();
    if (pid < 0) { LOGW("fork: %s", strerror(errno)); return; }
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        execlp(NOTIFY_SEND_BIN, NOTIFY_SEND_BIN,
               "--app-name=" PROG_NAME,
               "-u", urgency,
               "-i", sev_icon(sev),
               "--", title, body, NULL);
        _exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
        LOGW("notify-send exited with %d", WEXITSTATUS(status));
}

static void escape_html(const char *src, char *dst, size_t dstsz) {
    size_t i = 0;
    while (*src && i + 10 < dstsz) {
        if      (*src == '<') { memcpy(dst+i, "&lt;",  4); i += 4; }
        else if (*src == '>') { memcpy(dst+i, "&gt;",  4); i += 4; }
        else if (*src == '&') { memcpy(dst+i, "&amp;", 5); i += 5; }
        else                  { dst[i++] = *src; }
        src++;
    }
    dst[i] = '\0';
}

static void notify_telegram(const char *title, const char *body) {
    if (!g_cfg.telegram_token[0] || !g_cfg.telegram_chat_id[0]) {
        LOGW("Telegram: token or chat_id not configured");
        return;
    }
    if (access(CURL_BIN, X_OK) != 0) {
        LOGW("curl not found at %s", CURL_BIN);
        return;
    }

    char etitle[256], ebody[1024];
    escape_html(title, etitle, sizeof(etitle));
    escape_html(body,  ebody,  sizeof(ebody));

    char msg[1536];
    snprintf(msg, sizeof(msg), "<b>[%s] %s</b>\n%s", PROG_NAME, etitle, ebody);

    char url[768];
    snprintf(url, sizeof(url), "%s/bot%s/sendMessage",
             g_cfg.telegram_api_url, g_cfg.telegram_token);

    /* Build per-field args to avoid injection */
    char chat_id_param[128], text_param[1600];
    snprintf(chat_id_param, sizeof(chat_id_param), "chat_id=%s", g_cfg.telegram_chat_id);
    snprintf(text_param,    sizeof(text_param),    "text=%s",    msg);

    pid_t pid = fork();
    if (pid < 0) { LOGW("fork: %s", strerror(errno)); return; }
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        int devnull = open("/dev/null", O_RDWR | O_CLOEXEC);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execlp(CURL_BIN, CURL_BIN, "-sS", "--max-time", "10",
               "-X", "POST", url,
               "--data-urlencode", chat_id_param,
               "--data-urlencode", "parse_mode=HTML",
               "--data-urlencode", text_param,
               NULL);
        _exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
        LOGW("curl (Telegram) exited with %d", WEXITSTATUS(status));
    else
        LOGI("Telegram notification sent");
}

void notify_send(Severity sev, const char *title, const char *fmt, ...) {
    char body[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);

    /* Always log */
    int prio = (sev == SEV_CRIT) ? LOG_ERR :
               (sev == SEV_WARN) ? LOG_WARNING : LOG_INFO;
    log_msg(prio, sev_str(sev), "%s: %s", title, body);

    if (g_cfg.notify_libnotify) notify_libnotify(title, body, sev);
    if (g_cfg.notify_telegram)  notify_telegram(title, body);
}
