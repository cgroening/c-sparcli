#include "sparcli.h"
#include "internal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *rule_h[] = {
    [SC_BORDER_NONE]    = " ",
    [SC_BORDER_ASCII]   = "-",
    [SC_BORDER_SINGLE]  = "─",
    [SC_BORDER_DOUBLE]  = "═",
    [SC_BORDER_ROUNDED] = "─",
    [SC_BORDER_THICK]   = "━",
};

static void print_h_repeat(const char *h, int n, ScColor color) {
    if (n <= 0) return;
    int colored = (color.index != -2);
    if (colored) sc_apply_colors(color, SC_ANSI_COLOR_NONE);
    for (int i = 0; i < n; i++) fputs(h, stdout);
    if (colored) fputs("\033[0m", stdout);
}

void sc_rule_text(const ScText *title, ScRuleOpts opts) {
    int term_w = sc_terminal_width();
    int ml = opts.margin.left  > 0 ? opts.margin.left  : 0;
    int mr = opts.margin.right > 0 ? opts.margin.right : 0;
    int eff_w   = opts.width > 0 ? opts.width : (term_w - ml - mr);
    if (eff_w < 1) eff_w = 1;

    int left_pad = ml;
    if (opts.width > 0) {
        int avail = term_w - ml - mr;
        int spare = avail - eff_w;
        if (spare < 0) spare = 0;
        if (opts.align == SC_ALIGN_CENTER)     left_pad += spare / 2;
        else if (opts.align == SC_ALIGN_RIGHT) left_pad += spare;
    }

    int style = (opts.style >= SC_BORDER_NONE && opts.style <= SC_BORDER_THICK)
                ? opts.style : SC_BORDER_SINGLE;
    const char *h = rule_h[style];

    int has_title = title && title->count > 0;
    int title_w   = has_title ? (int)sc_text_visible_width(title) : 0;
    int tpad      = opts.title.pad > 0 ? opts.title.pad : (opts.title.pad == 0 ? 1 : 0);

    for (int i = 0; i < opts.margin.top; i++) fputc('\n', stdout);

    for (int i = 0; i < left_pad; i++) fputc(' ', stdout);

    if (has_title) {
        int dashes = eff_w - title_w - 2 * tpad;
        if (dashes < 0) dashes = 0;
        int ld, rd;
        if (opts.title.align == SC_ALIGN_LEFT) {
            ld = 1; rd = dashes - 1;
        } else if (opts.title.align == SC_ALIGN_RIGHT) {
            ld = dashes - 1; rd = 1;
        } else {
            ld = dashes / 2; rd = dashes - ld;
        }
        if (ld < 0) ld = 0;
        if (rd < 0) rd = 0;

        int colored = (opts.color.index != -2);
        print_h_repeat(h, ld, opts.color);
        if (colored) sc_apply_colors(opts.color, SC_ANSI_COLOR_NONE);
        for (int i = 0; i < tpad; i++) fputc(' ', stdout);
        if (colored) fputs("\033[0m", stdout);
        sc_print_text(title);
        if (colored) sc_apply_colors(opts.color, SC_ANSI_COLOR_NONE);
        for (int i = 0; i < tpad; i++) fputc(' ', stdout);
        if (colored) fputs("\033[0m", stdout);
        print_h_repeat(h, rd, opts.color);
    } else {
        print_h_repeat(h, eff_w, opts.color);
    }

    fputc('\n', stdout);

    for (int i = 0; i < opts.margin.bottom; i++) fputc('\n', stdout);
}

void sc_rule_str(const char *title, ScRuleOpts opts) {
    if (!title || !*title) {
        sc_rule_text(NULL, opts);
        return;
    }
    ScText *t = sc_text_new();
    sc_text_append(t, title, opts.title.opts);
    sc_rule_text(t, opts);
    sc_text_free(t);
}
