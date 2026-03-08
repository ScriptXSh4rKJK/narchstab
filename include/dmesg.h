#pragma once
#include "common.h"

/*
 * Parse dmesg output, classify hardware errors, MCE events, I/O errors.
 * Sends notifications for anything at or above the configured severity.
 *
 * Returns number of issues found, or -1 on error.
 */
int dmesg_run(void);
