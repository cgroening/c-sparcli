#pragma once

#include "sparcli_core.h"
#include "sparcli_text.h"

typedef struct {
    ScBorderStyle style;
    ScColor       color;
    ScOptions     title_opts;
    ScAlign       title_align;
    int           title_pad;
    int           width;    /* 0 = full terminal width */
    ScAlign       align;    /* placement of rule when width > 0 */
    int           margin;
    int           pad_y;
} ScRuleOpts;

void sc_rule_str (const char *title, ScRuleOpts opts);
void sc_rule_text(const ScText *title, ScRuleOpts opts);
