#pragma once

#include <stddef.h>

typedef struct {
    char  **lines;      /* heap-alloc strings, ANSI codes included */
    int    *vis_widths; /* visible character width per line */
    size_t  count;
    int     max_vis_w;  /* max vis_width across all lines */
} ScRendered;

void sc_rendered_free(ScRendered *r);
