#include "sparcli.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Frame tables ────────────────────────────────────────────────────────── */

static const char * const spinner_frames[4][10] = {
    [SC_SPINNER_BRAILLE] = {                              /* ⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏ */
        "\xe2\xa0\x8b", "\xe2\xa0\x99", "\xe2\xa0\xb9", "\xe2\xa0\xb8",
        "\xe2\xa0\xbc", "\xe2\xa0\xb4", "\xe2\xa0\xa6", "\xe2\xa0\xa7",
        "\xe2\xa0\x87", "\xe2\xa0\x8f"
    },
    [SC_SPINNER_PIPE]    = {                              /* |/-\ */
        "|", "/", "-", "\\", NULL, NULL, NULL, NULL, NULL, NULL
    },
    [SC_SPINNER_DOTS]    = {                              /* ⣾⣽⣻⢿⡿⣟⣯⣷ */
        "\xe2\xa3\xbe", "\xe2\xa3\xbd", "\xe2\xa3\xbb", "\xe2\xa2\xbf",
        "\xe2\xa1\xbf", "\xe2\xa3\x9f", "\xe2\xa3\xaf", "\xe2\xa3\xb7",
        NULL, NULL
    },
    [SC_SPINNER_ARROW]   = {                              /* ←↖↑↗→↘↓↙ */
        "\xe2\x86\x90", "\xe2\x86\x96", "\xe2\x86\x91", "\xe2\x86\x97",
        "\xe2\x86\x92", "\xe2\x86\x98", "\xe2\x86\x93", "\xe2\x86\x99",
        NULL, NULL
    },
};
static const int spinner_frame_count[] = { 10, 4, 8, 8 };

/* ── Internal structure ──────────────────────────────────────────────────── */

struct ScSpinner {
    ScSpinnerOpts  opts;
    char          *label;  /* owned */
    int            frame;
};

/* zero-initialised ScTextStyle = "no formatting" sentinel */
static int opts_no_fmt(ScTextStyle o) {
    return o.attr == 0 &&
           o.fg.index == 0 && !o.fg.r && !o.fg.g && !o.fg.b &&
           o.bg.index == 0 && !o.bg.r && !o.bg.g && !o.bg.b;
}

/* zero-initialised ScColor (index=0, rgb=0) treated as "not set" */
static int color_is_set(ScColor c) {
    return c.index != -2 && !(c.index == 0 && !c.r && !c.g && !c.b);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

ScSpinner *sc_spinner_new(const char *label, ScSpinnerOpts opts) {
    ScSpinner *s = calloc(1, sizeof(ScSpinner));
    s->opts  = opts;
    s->label = label ? strdup(label) : NULL;
    return s;
}

void sc_spinner_set_label(ScSpinner *s, const char *label) {
    free(s->label);
    s->label = label ? strdup(label) : NULL;
}

/* ── Rendering ───────────────────────────────────────────────────────────── */

void sc_spinner_tick(ScSpinner *s) {
    int         style = (int)s->opts.style;
    const char *frame = spinner_frames[style][s->frame];
    int         tw    = sc_term_width();

    /* spinner character */
    int colored = color_is_set(s->opts.color);
    if (colored) sc_apply_colors(s->opts.color, SC_ANSI_COLOR_NONE);
    fputs(frame, stdout);
    if (colored) fputs("\033[0m", stdout);

    int vis = 1;  /* spinner char = 1 visible column */

    /* label */
    if (s->label) {
        fputc(' ', stdout); vis++;
        int lw = (int)sc_utf8_vis_w(s->label, strlen(s->label));
        if (!opts_no_fmt(s->opts.label_opts))
            sc_print(s->label, s->opts.label_opts);
        else
            fputs(s->label, stdout);
        vis += lw;
    }

    /* pad to terminal width to overwrite any longer previous line */
    for (int i = vis; i < tw; i++) fputc(' ', stdout);

    fputc('\r', stdout);
    fflush(stdout);

    s->frame = (s->frame + 1) % spinner_frame_count[style];
}

void sc_spinner_finish(ScSpinner *s, int success, const char *label) {
    /* clear the current line */
    int tw = sc_term_width();
    for (int i = 0; i < tw; i++) fputc(' ', stdout);
    fputc('\r', stdout);

    /* ✔ (green) or ✖ (red) */
    ScColor sym_col = success ? SC_ANSI_COLOR_GREEN : SC_ANSI_COLOR_RED;
    sc_apply_colors(sym_col, SC_ANSI_COLOR_NONE);
    fputs(success ? "\xe2\x9c\x94" : "\xe2\x9c\x96", stdout);  /* ✔ / ✖ */
    fputs("\033[0m", stdout);

    if (label) {
        fputc(' ', stdout);
        fputs(label, stdout);
    }
    fputc('\n', stdout);
    (void)s;  /* s->opts could be used for label_opts in the future */
}

/* ── Memory management ───────────────────────────────────────────────────── */

void sc_spinner_free(ScSpinner *s) {
    if (!s) return;
    free(s->label);
    free(s);
}
