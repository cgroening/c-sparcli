#include "sparcli.h"
#include "internal.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


/** Left and right fill character counts for a titled rule line. */
typedef struct {
    int left;
    int right;
} FillSplit;

/** Render-time context for printing a rule line with an embedded title. */
typedef struct {
    ScRuleOpts    opts;
    const ScText *title;
    const char   *fill_char;
    int terminal_width;
    int effective_width;
    int title_width;
    int title_padding;
    int margin_left;
    int margin_right;
} Rule;


static const char *rule_h[] = {
    [SC_BORDER_NONE]    = " ",
    [SC_BORDER_ASCII]   = "-",
    [SC_BORDER_SINGLE]  = "─",
    [SC_BORDER_DOUBLE]  = "═",
    [SC_BORDER_ROUNDED] = "─",
    [SC_BORDER_THICK]   = "━",
};


// sc_rule_str
//     sc_rule_text
static int get_left_margin(ScRuleOpts opts);
static int get_right_margin(ScRuleOpts opts);
static int get_effective_width(ScRuleOpts opts, int terminal_width, int margin_left, int margin_right);
static int get_style(ScRuleOpts opts);
static int get_title_width(const ScText *title);
static int get_title_padding(ScRuleOpts opts);
static int get_left_pad(ScRuleOpts opts, int terminal_width, int margin_left, int margin_right, int effective_width);
static void print_rule_line_with_title(Rule ctx);
    static FillSplit get_fill_split(Rule ctx);
    static void print_rule_fill(const char *fill_char, int fill_char_count, ScColor color);


void sc_rule_text(const ScText *title, ScRuleOpts opts) {
    int terminal_width  = sc_terminal_width();
    int margin_left     = get_left_margin(opts);
    int margin_right    = get_right_margin(opts);
    int effective_width = get_effective_width(opts, terminal_width, margin_left, margin_right);

    Rule ctx = {
        .title           = title,
        .fill_char       = rule_h[get_style(opts)],
        .effective_width = effective_width,
        .title_width     = get_title_width(title),
        .title_padding   = get_title_padding(opts),
        .opts            = opts,
    };

    int left_pad  = get_left_pad(opts, terminal_width, margin_left, margin_right, effective_width);
    int has_title = title && title->count > 0;

    for (int i = 0; i < opts.margin.top; i++) { fputc('\n', stdout); }

    for (int i = 0; i < left_pad; i++) { fputc(' ', stdout); }

    if (has_title) {
        print_rule_line_with_title(ctx);
    } else {
        print_rule_fill(ctx.fill_char, ctx.effective_width, ctx.opts.color);
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
 * Prints one rule line with `ctx.title` embedded: fill chars on both sides,
 * separated from the title by `ctx.title_padding` spaces on each side.
 * The left/right fill counts are split according to `ctx.opts.title.align`.
 */
static void print_rule_line_with_title(Rule ctx) {
    FillSplit fs = get_fill_split(ctx);
    int colored  = (ctx.opts.color.index != -2);

    print_rule_fill(ctx.fill_char, fs.left, ctx.opts.color);
    if (colored) { sc_apply_colors(ctx.opts.color, SC_ANSI_COLOR_NONE); }
    for (int i = 0; i < ctx.title_padding; i++) { fputc(' ', stdout); }
    if (colored) { fputs("\033[0m", stdout); }
    sc_print_text(ctx.title);
    if (colored) { sc_apply_colors(ctx.opts.color, SC_ANSI_COLOR_NONE); }
    for (int i = 0; i < ctx.title_padding; i++) { fputc(' ', stdout); }
    if (colored) { fputs("\033[0m", stdout); }
    print_rule_fill(ctx.fill_char, fs.right, ctx.opts.color);
}

/**
 * Splits fill characters into left and right counts according to
 * `ctx.opts.title.align`; both values are clamped to `>= 0`.
 */
static FillSplit get_fill_split(Rule ctx) {
    int fill_total = ctx.effective_width - ctx.title_width - 2 * ctx.title_padding;
    if (fill_total < 0) { fill_total = 0; }

    FillSplit fs;
    if (ctx.opts.title.align == SC_ALIGN_LEFT) {
        fs.left = 1; fs.right = fill_total - 1;
    } else if (ctx.opts.title.align == SC_ALIGN_RIGHT) {
        fs.left = fill_total - 1; fs.right = 1;
    } else {
        fs.left = fill_total / 2; fs.right = fill_total - fs.left;
    }
    if (fs.left  < 0) { fs.left  = 0; }
    if (fs.right < 0) { fs.right = 0; }
    return fs;
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
