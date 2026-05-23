#pragma once

#include "sparcli_core.h"
#include "sparcli_text.h"


typedef struct {
    ScBorderStyle    border;
    ScColor          bg;            /* content area background; zero-init = none */
    ScTitle          title;
    ScEdges          padding;       /* inner content padding (top/right/bottom/left) */
    ScEdges          margin;        /* outer margin (top/right/bottom/left) */
    int              width;         /* 0 = auto */
    ScHAlign         content_align;
    int              full_width;    /* 1 = stretch to terminal width */
} ScPanelOpts;


void sc_panel_str (const char *content, ScPanelOpts opts);
void sc_panel_text(const ScText *content, ScPanelOpts opts);
