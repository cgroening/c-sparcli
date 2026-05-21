#pragma once

#include "sparcli_core.h"
#include "sparcli_text.h"

typedef struct {
    const char *left_cap;
    const char *right_cap;
    ScOptions   text_opts; /* zero-init = no formatting */
    int         pad;       /* spaces inside each cap, default 0 */
} ScBadgeOpts;

void sc_print_badge      (const char *text, ScBadgeOpts opts);
void sc_text_append_badge(ScText *t, const char *text, ScBadgeOpts opts);
