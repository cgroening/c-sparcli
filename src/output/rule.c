#include "sparcli.h"
#include "core/text_internal.h"
#include "internal.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


/** Render-time context for a single rule print call. */
typedef struct Rule {
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

/** Left and right fill character counts for a titled rule line. */
typedef struct {
    int left;
    int right;
} FillSplit;


static const char *rule_chars[] = {
    [SC_BORDER_NONE]    = " ",
    [SC_BORDER_ASCII]   = "-",
    [SC_BORDER_SINGLE]  = "─",
    [SC_BORDER_DOUBLE]  = "═",
    [SC_BORDER_ROUNDED] = "─",
    [SC_BORDER_THICK]   = "━",
};


static int get_left_margin(Rule *self);
static int get_right_margin(Rule *self);
static int get_effective_width(Rule *self);
static int get_style(Rule *self);
static int get_title_width(Rule *self);
static int get_title_padding(Rule *self);
static int get_indentation(Rule *self);
static void print_rule_line_with_title(Rule *self);
    static FillSplit get_fill_split(Rule *self);
    static void print_rule_fill(Rule *self, int count);


void sc_rule_text(const ScText *title, ScRuleOpts opts) {
    Rule self = { .opts = opts, .title = title };
    self.terminal_width  = sc_terminal_width();
    self.margin_left     = get_left_margin(&self);
    self.margin_right    = get_right_margin(&self);
    self.effective_width = get_effective_width(&self);
    self.fill_char       = rule_chars[get_style(&self)];
    self.title_width     = get_title_width(&self);
    self.title_padding   = get_title_padding(&self);

    // Print top margin and left padding
    for (int i = 0; i < opts.margin.top; i++) { fputc('\n', sc_output_stream()); }
    for (int i = 0; i < get_indentation(&self); i++) { fputc(' ', sc_output_stream()); }

    // Print rule line with or without title
    int has_title = title && title->count > 0;
    if (has_title) {
        print_rule_line_with_title(&self);
    } else {
        print_rule_fill(&self, self.effective_width);
    }
    fputc('\n', sc_output_stream());

    // Print bottom margin
    for (int i = 0; i < opts.margin.bottom; i++) { fputc('\n', sc_output_stream()); }
}


/** Returns the left margin, clamped to `>= 0`. */
static int get_left_margin(Rule *self) {
    return self->opts.margin.left > 0 ? self->opts.margin.left : 0;
}

/** Returns the right margin, clamped to `>= 0`. */
static int get_right_margin(Rule *self) {
    return self->opts.margin.right > 0 ? self->opts.margin.right : 0;
}

/**
 * Returns the effective render width: `opts.width` when set, otherwise
 * `terminal_width - margin_left - margin_right`, clamped to `>= 1`.
 */
static int get_effective_width(Rule *self) {
    int width;
    if (self->opts.width > 0) {
        width = self->opts.width;
    } else {
        width = self->terminal_width - self->margin_left - self->margin_right;
    }
    if (width < 1) { width = 1; }
    return width;
}

/**
 * Returns the validated border style index; falls back to `SC_BORDER_SINGLE`
 * when `opts.type` is out of range.
 */
static int get_style(Rule *self) {
    if (self->opts.type >= SC_BORDER_NONE &&
        self->opts.type <= SC_BORDER_THICK) {
        return self->opts.type;
    }
    return SC_BORDER_SINGLE;
}

/**
 * Returns the visible column width of `title`, or `0` when `title` is
 * `NULL` or empty.
 */
static int get_title_width(Rule *self) {
    if (self->title && self->title->count > 0) {
        return (int)sc_text_visible_width(self->title);
    }
    return 0;
}

/**
 * Returns the title padding: `opts.title.pad` when `> 0`, `1` when `== 0`
 * (default), `0` when `< 0` (explicitly disabled).
 */
static int get_title_padding(Rule *self) {
    if (self->opts.title.pad > 0) {
        return self->opts.title.pad;
    }

    if (self->opts.title.pad == 0) {
        return 1;
    }

    return 0;
}

/**
 * Returns the total left offset before the rule line: `margin_left` plus an
 * alignment offset when `opts.width > 0` distributes spare space.
 */
static int get_indentation(Rule *self) {
    int indentation = self->margin_left;
    if (self->opts.width > 0) {
        int spare = self->terminal_width - self->margin_left -
                    self->margin_right - self->effective_width;
        if (spare < 0) {
            spare = 0;
        }
        if (self->opts.align == SC_ALIGN_CENTER) {
            indentation += spare / 2;
        }
        else if (self->opts.align == SC_ALIGN_RIGHT) {
            indentation += spare;
        }
    }
    return indentation;
}

/**
 * Prints one rule line with `self->title` embedded: fill chars on both sides,
 * separated from the title by `self->title_padding` spaces on each side.
 * The left/right fill counts are split according to `self->opts.title.align`.
 */
static void print_rule_line_with_title(Rule *self) {
    FillSplit fill_split = get_fill_split(self);
    int is_colored = (self->opts.color.index != 0);

    print_rule_fill(self, fill_split.left);

    if (is_colored) { sc_apply_colors(self->opts.color, SC_ANSI_COLOR_NONE); }
    for (int i = 0; i < self->title_padding; i++) { fputc(' ', sc_output_stream()); }
    if (is_colored) { fputs("\033[0m", sc_output_stream()); }

    sc_print_text(self->title);

    if (is_colored) { sc_apply_colors(self->opts.color, SC_ANSI_COLOR_NONE); }
    for (int i = 0; i < self->title_padding; i++) { fputc(' ', sc_output_stream()); }
    if (is_colored) { fputs("\033[0m", sc_output_stream()); }

    print_rule_fill(self, fill_split.right);
}

/**
 * Splits fill characters into left and right counts according to
 * `self->opts.title.align`; both values are clamped to `>= 0`.
 */
static FillSplit get_fill_split(Rule *self) {
    int fill_total = self->effective_width - self->title_width -
                     2 * self->title_padding;
    if (fill_total < 0) {
        fill_total = 0;
    }

    FillSplit fill_split;
    if (self->opts.title.align == SC_ALIGN_LEFT) {
        fill_split.left = 1;
        fill_split.right = fill_total - 1;
    } else if (self->opts.title.align == SC_ALIGN_RIGHT) {
        fill_split.left = fill_total - 1;
        fill_split.right = 1;
    } else {
        fill_split.left = fill_total / 2;
        fill_split.right = fill_total - fill_split.left;

    }
    if (fill_split.left < 0) {
        fill_split.left = 0;
    }
    if (fill_split.right < 0) {
        fill_split.right = 0;
    }
    return fill_split;
}

/**
 * Prints `self->fill_char` exactly `count` times, wrapped in ANSI color
 * escapes when `self->opts.color` is set.
 *
 * @param count  Number of repetitions; no output when `<= 0`.
 */
static void print_rule_fill(Rule *self, int count) {
    if (count <= 0) {
        return;
    }

    int is_colored = (self->opts.color.index != 0);
    if (is_colored) {
        sc_apply_colors(self->opts.color, SC_ANSI_COLOR_NONE);
    }
    for (int i = 0; i < count; i++) {
        fputs(self->fill_char, sc_output_stream());
    }
    if (is_colored) {
        fputs("\033[0m", sc_output_stream());
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
