#pragma once
#include "common.h"

/*
 * Enumerate failed systemd units, pull recent journal entries for each,
 * and produce a human-readable report.
 *
 * Returns number of failed units found, or -1 on error.
 */
int report_run(void);
