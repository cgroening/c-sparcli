#pragma once

#include "sparcli_core.h"
#include "sparcli_text.h"

typedef struct {
    ScBorderStyle border;
    ScColor       border_color;
    const char   *title;         /* NULL = no title */
    ScOptions     title_opts;
    ScTitlePos    title_pos;
    ScAlign       title_align;
    int           pad_x;
    int           pad_y;
    int           width;         /* 0 = auto */
    int           title_pad;     /* spaces on each side of title text, default 1 */
    ScAlign       content_align;
    int           full_width;    /* 1 = stretch to terminal width */
} ScPanelOpts;

void sc_panel_str (const char *content, ScPanelOpts opts);
void sc_panel_text(const ScText *content, ScPanelOpts opts);
