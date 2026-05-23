#pragma once

#include "sparcli_core.h"
#include "sparcli_text.h"

typedef struct {
    ScBorderType style;
    ScColor       color;
    ScTextStyle    title_style;
    ScHAlign       title_align;
    int           title_pad;
    int           width;    /* 0 = full terminal width */
    ScHAlign       align;    /* placement of rule when width > 0 */
    ScEdges       margin;        /* top/bottom = blank lines; left/right = horizontal indent */
} ScRuleOpts;

void sc_rule_str (const char *title, ScRuleOpts opts);
void sc_rule_text(const ScText *title, ScRuleOpts opts);
