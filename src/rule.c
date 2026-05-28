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


static int get_left_margin(ScRuleOpts opts);
static int get_right_margin(ScRuleOpts opts);
static int get_effective_width(
    ScRuleOpts opts, int terminal_width, int margin_left, int margin_right
);
static int get_style(ScRuleOpts opts);
static int get_title_width(const ScText *title);
static int get_title_padding(ScRuleOpts opts);
static int get_left_pad(ScRuleOpts opts, int terminal_width, int margin_left, int margin_right, int effective_width);
static void print_rule_fill(
    const char *fill_char, int fill_char_count, ScColor color
);


void sc_rule_text(const ScText *title, ScRuleOpts opts) {
    int terminal_width  = sc_terminal_width();
    int margin_left     = get_left_margin(opts);
    int margin_right    = get_right_margin(opts);
    int effective_width = get_effective_width(
        opts, terminal_width, margin_left, margin_right
    );
    int style             = get_style(opts);
    const char *fill_char = rule_h[style];

    int has_title   = title && title->count > 0;
    int title_width = get_title_width(title);
    int title_padding = get_title_padding(opts);

    int left_pad = get_left_pad(opts, terminal_width, margin_left, margin_right, effective_width);

    for (int i = 0; i < opts.margin.top; i++) { fputc('\n', stdout); }

    for (int i = 0; i < left_pad; i++) { fputc(' ', stdout); }

    if (has_title) {
        int dashes = effective_width - title_width - 2 * title_padding;
        if (dashes < 0) { dashes = 0; }
        int ld, rd;
        if (opts.title.align == SC_ALIGN_LEFT) {
            ld = 1; rd = dashes - 1;
        } else if (opts.title.align == SC_ALIGN_RIGHT) {
            ld = dashes - 1; rd = 1;
        } else {
            ld = dashes / 2; rd = dashes - ld;
        }
        if (ld < 0) { ld = 0; }
        if (rd < 0) { rd = 0; }

        int colored = (opts.color.index != -2);
        print_rule_fill(fill_char, ld, opts.color);
        if (colored) { sc_apply_colors(opts.color, SC_ANSI_COLOR_NONE); }
        for (int i = 0; i < title_padding; i++) { fputc(' ', stdout); }
        if (colored) { fputs("\033[0m", stdout); }
        sc_print_text(title);
        if (colored) { sc_apply_colors(opts.color, SC_ANSI_COLOR_NONE); }
        for (int i = 0; i < title_padding; i++) { fputc(' ', stdout); }
        if (colored) { fputs("\033[0m", stdout); }
        print_rule_fill(fill_char, rd, opts.color);
    } else {
        print_rule_fill(fill_char, effective_width, opts.color);
    }

    fputc('\n', stdout);

    for (int i = 0; i < opts.margin.bottom; i++) { fputc('\n', stdout); }
}


/** Returns the left margin, clamped to `>= 0`. */
static int get_left_margin(ScRuleOpts opts) {
    return opts.margin.left > 0 ? opts.margin.left : 0;
}

/** Returns the right margin, clamped to `>= 0`. */
static int get_right_margin(ScRuleOpts opts) {
    return opts.margin.right > 0 ? opts.margin.right : 0;
}

/**
 * Returns the effective render width: `opts.width` when set, otherwise
 * `terminal_width - margin_left - margin_right`, clamped to `>= 1`.
 */
static int get_effective_width(
    ScRuleOpts opts, int terminal_width, int margin_left, int margin_right
) {
    int w;
    if (opts.width > 0) {
        w = opts.width;
    } else {
        w = terminal_width - margin_left - margin_right;
    }
    if (w < 1) { w = 1; }
    return w;
}

/**
 * Returns the validated border style index; falls back to `SC_BORDER_SINGLE`
 * when `opts.border_type` is out of range.
 */
static int get_style(ScRuleOpts opts) {
    if (opts.border_type >= SC_BORDER_NONE && opts.border_type <= SC_BORDER_THICK) {
        return opts.border_type;
    }
    return SC_BORDER_SINGLE;
}

/**
 * Returns the visible column width of `title`, or `0` when `title` is
 * `NULL` or empty.
 */
static int get_title_width(const ScText *title) {
    if (title && title->count > 0) {
        return (int)sc_text_visible_width(title);
    }
    return 0;
}

/**
 * Returns the title padding: `opts.title.pad` when `> 0`, `1` when `== 0`
 * (default), `0` when `< 0` (explicitly disabled).
 */
static int get_title_padding(ScRuleOpts opts) {
    if (opts.title.pad > 0) { return opts.title.pad; }
    if (opts.title.pad == 0) { return 1; }
    return 0;
}

/**
 * Returns the total left offset before the rule line: `margin_left` plus an
 * alignment offset when `opts.width > 0` distributes spare space.
 */
static int get_left_pad(
    ScRuleOpts opts, int terminal_width, int margin_left, int margin_right,
    int effective_width
) {
    int left_pad = margin_left;
    if (opts.width > 0) {
        int spare = (terminal_width - margin_left - margin_right) - effective_width;
        if (spare < 0) { spare = 0; }
        if (opts.align == SC_ALIGN_CENTER)     { left_pad += spare / 2; }
        else if (opts.align == SC_ALIGN_RIGHT) { left_pad += spare; }
    }
    return left_pad;
}

/**
 * Prints the horizontal `fill_char` exactly `fill_char_count` times,
 * wrapped in ANSI color escapes when `color` is set.
 *
 * @param fill_char        UTF-8 box-drawing character to repeat (e.g. `"─"`, `"-"`).
 * @param fill_char_count  Number of repetitions; no output when `<= 0`.
 * @param color            Line color; `SC_ANSI_COLOR_NONE` = no escape codes.
 */
static void print_rule_fill(
    const char *fill_char, int fill_char_count, ScColor color
) {
    if (fill_char_count <= 0) {
        return;
    }

    int is_colored = (color.index != -2);
    if (is_colored) {
        sc_apply_colors(color, SC_ANSI_COLOR_NONE);
    }
    for (int i = 0; i < fill_char_count; i++) {
        fputs(fill_char, stdout);
    }
    if (is_colored) {
        fputs("\033[0m", stdout);
    }
}




void sc_rule_str(const char *title, ScRuleOpts opts) {
    if (!title || !*title) {
        sc_rule_text(NULL, opts);
        return;
    }
    ScText *t = sc_text_new();
    sc_text_append(t, title, opts.title.style);
    sc_rule_text(t, opts);
    sc_text_free(t);
}
