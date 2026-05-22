#pragma once

#include "sparcli_core.h"
#include "sparcli_text.h"

typedef struct {
    ScBorderType border;
    ScColor       border_color;
    ScColor       border_bg;     /* background color for border characters; SC_ANSI_COLOR_NONE = none */
    ScColor       bg;            /* background color for content area; SC_ANSI_COLOR_NONE = none */
    const char   *title;         /* NULL = no title */
    ScTextStyle     title_opts;
    ScTitlePosition    title_pos;
    ScHAlign       title_align;
    int           pad_x;
    int           pad_y;
    int           width;         /* 0 = auto */
    int           title_pad;     /* spaces on each side of title text, default 1 */
    ScHAlign       content_align;
    int           full_width;    /* 1 = stretch to terminal width */
} ScPanelOpts;

void sc_panel_str (const char *content, ScPanelOpts opts);
void sc_panel_text(const ScText *content, ScPanelOpts opts);
