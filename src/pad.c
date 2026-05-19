#include "sparcli.h"
#include "internal.h"
#include <stdio.h>

/* ── sc_pad_print ────────────────────────────────────────────────────────── */

void sc_pad_print(const ScRendered *r, ScPadOpts opts) {
    if (!r) return;
    for (int i = 0; i < opts.top; i++) fputc('\n', stdout);
    for (size_t i = 0; i < r->count; i++) {
        for (int k = 0; k < opts.left;  k++) fputc(' ', stdout);
        fputs(r->lines[i], stdout);
        for (int k = 0; k < opts.right; k++) fputc(' ', stdout);
        fputc('\n', stdout);
    }
    for (int i = 0; i < opts.bottom; i++) fputc('\n', stdout);
}

/* ── sc_align_print ──────────────────────────────────────────────────────── */

void sc_align_print(const ScRendered *r, ScAlign align, int width) {
    if (!r) return;
    if (width <= 0) width = sc_term_width();
    for (size_t i = 0; i < r->count; i++) {
        int vw    = r->vis_widths[i];
        int spare = width - vw;
        if (spare < 0) spare = 0;
        int lp = 0;
        if      (align == SC_ALIGN_CENTER) lp = spare / 2;
        else if (align == SC_ALIGN_RIGHT)  lp = spare;
        for (int k = 0; k < lp; k++) fputc(' ', stdout);
        fputs(r->lines[i], stdout);
        fputc('\n', stdout);
    }
}

/* ── Convenience wrappers ────────────────────────────────────────────────── */

void sc_pad_str(const char *s, ScPadOpts opts) {
    ScRendered *r = sc_capture_str(s);
    sc_pad_print(r, opts);
    sc_rendered_free(r);
}

void sc_pad_text(const ScText *t, ScPadOpts opts) {
    ScRendered *r = sc_capture_text(t);
    sc_pad_print(r, opts);
    sc_rendered_free(r);
}

void sc_align_str(const char *s, ScAlign align, int width) {
    ScRendered *r = sc_capture_str(s);
    sc_align_print(r, align, width);
    sc_rendered_free(r);
}

void sc_align_text(const ScText *t, ScAlign align, int width) {
    ScRendered *r = sc_capture_text(t);
    sc_align_print(r, align, width);
    sc_rendered_free(r);
}
