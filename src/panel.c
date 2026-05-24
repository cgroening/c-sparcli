#include "sparcli.h"
#include "internal.h"
#include <string.h>
#include <stdlib.h>


/**
 * HBorder – Bundles the arguments passed to `render_horizontal_border`.
 */
typedef struct {
    ScBorderStyle border_style;  /**< Border style (type, color and bg) */
    int           inner_width;   /**< Number of chars between the edge chars. */
    char         *left_edge_character;   /**< Left corner character */
    char         *right_edge_character;  /**< Right corner character */
} HBorder;

/** PSpan – Single styled text segment within a content line of a panel. */
typedef struct {
    const char *text;  /**< Heap-allocated UTF-8 string (owned) */
    ScTextStyle opts;  /**< Style to apply when rendering `text` */
} PSpan;

/**
 * PLine – One line of panel content, parsed from `ScText` on `\n` boundaries.
 *
 * Each line is made up of one or more `PSpan` segments.
 */
typedef struct {
    PSpan  *spans;       /**< Array of styled segments that make up this line */
    size_t  count;       /**< Number of spans in `spans` */
    size_t  line_width;  /**< Visible width of the line in columns */
} PLine;

/**
 * PLineLayout – Layout parameters for a content line within the panel interior.
 */
typedef struct {
    int      inner_width; /**< Chars between the vertical edge characters */
    int      pad_l;       /**< Left content padding in columns */
    int      pad_r;       /**< Right content padding in columns */
    ScHAlign align;       /**< Horizontal alignment of the content */
} PLineLayout;

/**
 * PLineView – All parameters needed to render one panel row (content or empty).
 */
typedef struct {
    PLineLayout   layout; /**< Interior width, padding, and alignment */
    ScBorderStyle border; /**< Border style for the vertical edge characters */
    ScColor       bg;     /**< Content area background color */
    PLine        *line;   /**< Content spans; NULL for empty rows */
} PLineView;


static bool color_active(ScColor c);
static ScColor norm_bg(ScColor c);
static void print_colored(const char *s, ScBorderStyle style);
static void print_repeat(const char *s, int n, ScBorderStyle style);
static void render_horizontal_border(HBorder hborder, ScTitle title);
static void render_empty_line(PLineView row);
static PLine *make_plines(const ScText *t, size_t *out_n);
static void free_plines(PLine *lines, size_t n);
static void render_content_line(PLineView row);


/**
 * Maps each `ScBorderType` to its six box-drawing characters.
 *
 * `tl`/`tr`/`bl`/`br` = corners; `h` = horizontal char; `v` = vertical char.
 */
static const struct {
    const char *tl, *tr, *bl, *br, *h, *v;
} border_table[] = {
    [SC_BORDER_NONE]    = { " ", " ", " ", " ", " ", " " },
    [SC_BORDER_ASCII]   = { "+", "+", "+", "+", "-", "|" },
    [SC_BORDER_SINGLE]  = { "┌", "┐", "└", "┘", "─", "│" },
    [SC_BORDER_DOUBLE]  = { "╔", "╗", "╚", "╝", "═", "║" },
    [SC_BORDER_ROUNDED] = { "╭", "╮", "╰", "╯", "─", "│" },
    [SC_BORDER_THICK]   = { "┏", "┓", "┗", "┛", "━", "┃" },
};


void sc_panel_text(const ScText *text, ScPanelOpts opts) {
    size_t nlines;
    PLine *lines = make_plines(text, &nlines);

    size_t max_cw = 0;
    for (size_t i = 0; i < nlines; i++)
        if (lines[i].line_width > max_cw) max_cw = lines[i].line_width;

    int title_len = opts.title.text
        ? (int)sc_utf8_string_length(opts.title.text,
                                     strlen(opts.title.text))
        : 0;
    int min4title = opts.title.text
        ? title_len + 2 * opts.title.pad + 2 : 0;
    int pad_l = opts.padding.left  > 0 ? opts.padding.left  : 0;
    int pad_r = opts.padding.right > 0 ? opts.padding.right : 0;
    int pad_t = opts.padding.top   > 0 ? opts.padding.top   : 0;
    int pad_b = opts.padding.bottom> 0 ? opts.padding.bottom: 0;
    int ml    = opts.margin.left   > 0 ? opts.margin.left   : 0;
    int mr    = opts.margin.right  > 0 ? opts.margin.right  : 0;

    int inner_w;
    if (opts.full_width) {
        inner_w = sc_terminal_width() - 2 - ml - mr;
        if (inner_w < 2) inner_w = 2;
    } else if (opts.width > 0) {
        inner_w = opts.width - 2;
    } else {
        int from_content = (int)max_cw + pad_l + pad_r;
        inner_w = from_content > min4title ? from_content : min4title;
        if (inner_w < 2) inner_w = 2;
    }

    int bt = opts.border.type;
    HBorder top = {
        .border_style         = opts.border,
        .inner_width          = inner_w,
        .left_edge_character  = (char *)border_table[bt].tl,
        .right_edge_character = (char *)border_table[bt].tr,
    };
    HBorder bot = {
        .border_style         = opts.border,
        .inner_width          = inner_w,
        .left_edge_character  = (char *)border_table[bt].bl,
        .right_edge_character = (char *)border_table[bt].br,
    };

#define PMARG() \
    do { for (int _i = 0; _i < ml; _i++) fputc(' ', stdout); } while(0)

    for (int i = 0; i < opts.margin.top; i++) fputc('\n', stdout);

    /* top border */
    PMARG();
    ScTitle ttop = opts.title;
    if (opts.title.pos != SC_TITLE_TOP) ttop.text = NULL;
    render_horizontal_border(top, ttop);

    PLineView row = {
        .layout = { inner_w, pad_l, pad_r, opts.content_align },
        .border = opts.border,
        .bg     = opts.bg,
    };

    for (int i = 0; i < pad_t; i++) { PMARG(); render_empty_line(row); }

    for (size_t i = 0; i < nlines; i++) {
        row.line = &lines[i];
        PMARG();
        render_content_line(row);
    }

    for (int i = 0; i < pad_b; i++) { PMARG(); render_empty_line(row); }

    /* bottom border */
    PMARG();
    ScTitle tbot = opts.title;
    if (opts.title.pos != SC_TITLE_BOTTOM) tbot.text = NULL;
    render_horizontal_border(bot, tbot);

    for (int i = 0; i < opts.margin.bottom; i++) fputc('\n', stdout);

#undef PMARG

    free_plines(lines, nlines);
}

void sc_panel_str(const char *raw_str, ScPanelOpts opts) {
    ScText *t = sc_text_from_str(raw_str);
    sc_panel_text(t, opts);
    sc_text_free(t);
}


/**
 * Returns true if `color` should cause ANSI color escape codes to be emitted.
 *
 * Returns false for `SC_ANSI_COLOR_NONE` (index == -2) and for zero-initialized
 * `ScColor` structs. Zero-init ({index=0, r=0, g=0, b=0}) is treated as "not
 * set" because it is indistinguishable from `SC_ANSI_COLOR_BLACK`. Use
 * `sc_ansi_color_from_rgb(0,0,0)` to specify an explicit black.
 */
static bool color_active(ScColor color) {
    bool is_none     = color.index == -2;
    bool is_zeroinit = color.index == 0 && !color.r && !color.g && !color.b;
    return !is_none && !is_zeroinit;
}

/** Applies `style.color`/`style.bg`, prints `s`, then emits a reset. */
static void print_colored(const char *s, ScBorderStyle style) {
    sc_apply_colors(style.color, norm_bg(style.bg));
    fputs(s, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

/** Applies `style.color`/`style.bg`, prints `s` `n` times, then emits a reset. */
static void print_repeat(const char *s, int n, ScBorderStyle style) {
    if (n <= 0) return;
    sc_apply_colors(style.color, norm_bg(style.bg));
    for (int i = 0; i < n; i++) fputs(s, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

/**
 * Returns `SC_ANSI_COLOR_NONE` for the zero-init sentinel; else `color`.
 */
static ScColor norm_bg(ScColor color) {
    return color_active(color) ? color : SC_ANSI_COLOR_NONE;
}

/**
 * Renders a horizontal border line with an optional inline title.
 *
 * Distributes remaining dashes around the title per `title.align`,
 * guaranteeing at least one dash on each side. Pad spaces are printed
 * using the title style's own background color.
 */
static void render_horizontal_border(HBorder hborder, ScTitle title) {
    const char *h = border_table[hborder.border_style.type].h;
    if (title.pad < 0) title.pad = 0;
    ScBorderStyle title_pad_style = { 0, SC_ANSI_COLOR_NONE, title.style.bg };
    print_colored(hborder.left_edge_character, hborder.border_style);

    if (title.text && *title.text) {
        int tlen   = (int)sc_utf8_string_length(title.text,
                                                strlen(title.text));
        int dashes = hborder.inner_width - tlen - 2 * title.pad;
        if (dashes < 0) dashes = 0;

        int ld, rd;
        if (title.align == SC_ALIGN_LEFT) {
            ld = 1; rd = dashes - 1;
        } else if (title.align == SC_ALIGN_RIGHT) {
            ld = dashes - 1; rd = 1;
        } else { /* CENTER */
            ld = dashes / 2; rd = dashes - ld;
        }
        if (ld < 0) ld = 0;
        if (rd < 0) rd = 0;

        print_repeat(h, ld, hborder.border_style);
        for (int i = 0; i < title.pad; i++)
            print_colored(" ", title_pad_style);
        sc_print(title.text, title.style);
        for (int i = 0; i < title.pad; i++)
            print_colored(" ", title_pad_style);
        print_repeat(h, rd, hborder.border_style);
    } else {
        print_repeat(h, hborder.inner_width, hborder.border_style);
    }

    print_colored(hborder.right_edge_character, hborder.border_style);
    fputc('\n', stdout);
}

/** Renders one border-enclosed blank row, filling the interior with `row.bg` if active. */
static void render_empty_line(PLineView row) {
    print_colored(border_table[row.border.type].v, row.border);
    if (color_active(row.bg)) {
        sc_apply_colors(SC_ANSI_COLOR_NONE, row.bg);
        for (int i = 0; i < row.layout.inner_width; i++) fputc(' ', stdout);
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    } else {
        for (int i = 0; i < row.layout.inner_width; i++) fputc(' ', stdout);
    }
    print_colored(border_table[row.border.type].v, row.border);
    fputc('\n', stdout);
}

/**
 * Splits the spans of `t` on newline boundaries into an array of `PLine`
 * records. Each `PLine` owns heap copies of its span strings.
 * Caller must free the result with `free_plines`.
 */
static PLine *make_plines(const ScText *t, size_t *out_n) {
    size_t lines_cap = 8, nlines = 0;
    PLine *lines = malloc(lines_cap * sizeof(PLine));

    size_t buf_cap = 8, buf_n = 0, buf_w = 0;
    PSpan *buf = malloc(buf_cap * sizeof(PSpan));

    for (size_t si = 0; si < t->count; si++) {
        const char *s     = t->spans[si].raw_str;
        ScTextStyle   opts  = t->spans[si].style;
        const char *start = s;

        while (*s) {
            if (*s == '\n') {
                size_t seglen = (size_t)(s - start);
                if (seglen > 0) {
                    if (buf_n == buf_cap) {
                        buf_cap *= 2;
                        buf = realloc(buf, buf_cap * sizeof(PSpan));
                    }
                    buf[buf_n++] = (PSpan){ strndup(start, seglen), opts };
                    buf_w += sc_utf8_string_length(start, seglen);
                }
                /* flush line */
                if (nlines == lines_cap) {
                    lines_cap *= 2;
                    lines = realloc(lines, lines_cap * sizeof(PLine));
                }
                PSpan *ls = malloc((buf_n + 1) * sizeof(PSpan));
                memcpy(ls, buf, buf_n * sizeof(PSpan));
                lines[nlines++] = (PLine){ ls, buf_n, buf_w };
                buf_n = 0;
                buf_w = 0;
                start = s + 1;
            }
            s++;
        }
        size_t seglen = (size_t)(s - start);
        if (seglen > 0) {
            if (buf_n == buf_cap) {
                buf_cap *= 2;
                buf = realloc(buf, buf_cap * sizeof(PSpan));
            }
            buf[buf_n++] = (PSpan){ strndup(start, seglen), opts };
            buf_w += sc_utf8_string_length(start, seglen);
        }
    }

    /* flush last line */
    if (nlines == lines_cap) {
        lines_cap *= 2;
        lines = realloc(lines, lines_cap * sizeof(PLine));
    }
    PSpan *ls = malloc((buf_n + 1) * sizeof(PSpan));
    memcpy(ls, buf, buf_n * sizeof(PSpan));
    lines[nlines++] = (PLine){ ls, buf_n, buf_w };

    free(buf);
    *out_n = nlines;
    return lines;
}

/** Frees the span strings and line array produced by `make_plines`. */
static void free_plines(PLine *lines, size_t n) {
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < lines[i].count; j++)
            free((char *)lines[i].spans[j].text);
        free(lines[i].spans);
    }
    free(lines);
}

/**
 * Renders one content row with left/right padding and alignment spacing.
 *
 * Re-applies `row.bg` after each `sc_print` call because `sc_print`
 * always emits a reset that would otherwise clear the background color.
 */
static void render_content_line(PLineView row) {
    PLineLayout l = row.layout;
    int spare = l.inner_width - l.pad_l - l.pad_r - (int)row.line->line_width;
    if (spare < 0) spare = 0;
    int lp = 0, rp = spare;
    if (l.align == SC_ALIGN_CENTER) { lp = spare / 2; rp = spare - lp; }
    else if (l.align == SC_ALIGN_RIGHT) { lp = spare; rp = 0; }

    print_colored(border_table[row.border.type].v, row.border);
    if (color_active(row.bg))
        sc_apply_colors(SC_ANSI_COLOR_NONE, row.bg);
    for (int i = 0; i < l.pad_l; i++) fputc(' ', stdout);
    for (int i = 0; i < lp; i++) fputc(' ', stdout);
    for (size_t i = 0; i < row.line->count; i++) {
        sc_print(row.line->spans[i].text, row.line->spans[i].opts);
        if (color_active(row.bg))
            sc_apply_colors(SC_ANSI_COLOR_NONE, row.bg);
    }
    for (int i = 0; i < rp; i++) fputc(' ', stdout);
    for (int i = 0; i < l.pad_r; i++) fputc(' ', stdout);
    if (color_active(row.bg))
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    print_colored(border_table[row.border.type].v, row.border);
    fputc('\n', stdout);
}

