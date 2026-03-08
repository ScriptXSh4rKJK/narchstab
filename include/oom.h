#pragma once
#include "common.h"

/*
 * Detect available OOM killer backends, pick the best one (or honour config),
 * write appropriate configuration and enable the service.
 *
 * Returns 0 on success, -1 on error.
 */
int oom_run(void);
