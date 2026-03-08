#pragma once
#include "common.h"

typedef enum {
    SEV_INFO = 0,
    SEV_WARN,
    SEV_CRIT,
} Severity;

/* Send a notification through all configured channels */
void notify_send(Severity sev, const char *title, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
