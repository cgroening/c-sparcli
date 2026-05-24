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


/**
 * Panel – Internal rendering context for a panel.
 *
 * Holds both the caller-supplied inputs and all derived layout values so that
 * helper functions only need a single `Panel *` argument.
 */
typedef struct {
    /** Content to render inside the panel (not owned) */
    const ScText *text;

    /** Layout and visual options (border, title, width, …) */
    ScPanelOpts   opts;

    /** Parsed content lines split from `text` on `\n` boundaries (owned) */
    PLine        *lines;

    /** Number of elements in `lines` */
    size_t        line_count;

    /** Padding and margin values clamped to >= 0 */
    ScSpacing     spacing;

    /** Visible width in columns of the widest content line */
    size_t        max_line_width;

    /** Chars between the left and right vertical border chars */
    int           inner_width;

    /** Top horizontal border (corners, inner width, style) */
    HBorder       top_border;

    /** Bottom horizontal border (corners, inner width, style) */
    HBorder       bottom_border;

    /** Row rendering template; `line` field is set per row during rendering */
    PLineView     line_view_template;
} Panel;


static PLine *split_text_into_panel_lines(
    const ScText *text, size_t *out_line_count
);
static void print_margin(int n);
static bool color_active(ScColor c);
static ScColor norm_bg(ScColor c);
static void print_colored(const char *s, ScBorderStyle style);
static void print_repeat(const char *s, int n, ScBorderStyle style);
static ScSpacing resolve_panel_spacing(ScPanelOpts opts);
static size_t get_max_line_width(PLine *lines, size_t line_count);
static int compute_inner_width(ScPanelOpts opts, size_t max_cw, ScSpacing sp);
static HBorder make_hborder(
    ScBorderStyle style, int inner_w, const char *l, const char *r
);
static void render_horizontal_border(HBorder hborder, ScTitle title);
static void add_vertical_margin(int line_count);
static void render_panel_border(
    HBorder hborder, ScTitle title, ScTitlePosition pos, int ml
);
static void render_empty_line(PLineView row);
static void render_content_line(PLineView row);
static void render_body(PLineView row, PLine *lines, size_t count, ScSpacing sp);
static void free_plines(PLine *lines, size_t n);


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
    //  Parse text and resolve spacing parameters
    size_t line_count;
    PLine *lines = split_text_into_panel_lines(text, &line_count);
    ScSpacing sp = resolve_panel_spacing(opts);
    size_t max_line_width = get_max_line_width(lines, line_count);
    int inner_width = compute_inner_width(opts, max_line_width, sp);

    // Create the top and bottom horizontal borders based on the border type
    // and inner width
    int bt = opts.border.type;
    HBorder top = make_hborder(
        opts.border, inner_width, border_table[bt].tl, border_table[bt].tr
    );
    HBorder bot = make_hborder(
        opts.border, inner_width, border_table[bt].bl, border_table[bt].br
    );

    // Create an empty line view to be used as a template for all lines,
    // since they all share the same layout, border and background style
    PLineView line_view_template = {
        .layout = {
            inner_width, sp.padding.left, sp.padding.right, opts.content_align
        },
        .border = opts.border,
        .bg     = opts.bg,
    };

    // Print the panel
    add_vertical_margin(sp.margin.top);
    render_panel_border(top, opts.title, SC_TITLE_TOP, sp.margin.left);
    render_body(line_view_template, lines, line_count, sp);
    render_panel_border(bot, opts.title, SC_TITLE_BOTTOM, sp.margin.left);
    add_vertical_margin(sp.margin.bottom);
    free_plines(lines, line_count);
}

void sc_panel_str(const char *raw_str, ScPanelOpts opts) {
    ScText *t = sc_text_from_str(raw_str);
    sc_panel_text(t, opts);
    sc_text_free(t);
}

/**
 * Splits the spans of `text` on newline boundaries into an array of `PLine`
 * records. Each `PLine` owns heap copies of its span strings.
 * Caller must free the result with `free_plines`.
 */
static PLine *split_text_into_panel_lines(const ScText *text, size_t *out_line_count) {
    size_t lines_cap = 8, nlines = 0;
    PLine *lines = malloc(lines_cap * sizeof(PLine));

    size_t buf_cap = 8, buf_n = 0, buf_w = 0;
    PSpan *buf = malloc(buf_cap * sizeof(PSpan));

    for (size_t si = 0; si < text->count; si++) {
        const char *s     = text->spans[si].raw_str;
        ScTextStyle   opts  = text->spans[si].style;
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
    *out_line_count = nlines;
    return lines;
}

/** Prints `n` space characters to stdout (left margin). */
static void print_margin(int n) {
    for (int i = 0; i < n; i++) fputc(' ', stdout);
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

/** Returns `SC_ANSI_COLOR_NONE` for the zero-init sentinel; else `color`. */
static ScColor norm_bg(ScColor color) {
    return color_active(color) ? color : SC_ANSI_COLOR_NONE;
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

/** Clamps all padding and margin values from `opts` to zero or above. */
static ScSpacing resolve_panel_spacing(ScPanelOpts opts) {
    return (ScSpacing){
        .padding = {
            .left   = opts.padding.left   > 0 ? opts.padding.left   : 0,
            .right  = opts.padding.right  > 0 ? opts.padding.right  : 0,
            .top    = opts.padding.top    > 0 ? opts.padding.top    : 0,
            .bottom = opts.padding.bottom > 0 ? opts.padding.bottom : 0,
        },
        .margin = {
            .left   = opts.margin.left   > 0 ? opts.margin.left   : 0,
            .right  = opts.margin.right  > 0 ? opts.margin.right  : 0,
            .top    = opts.margin.top    > 0 ? opts.margin.top    : 0,
            .bottom = opts.margin.bottom > 0 ? opts.margin.bottom : 0,
        },
    };
}

/**
 * Iterates the `lines` to find the maximum `line_width`, which is the visible
 * width of the longest line in columns. This is used to compute the panel inner
 * width when `opts.width` is not set.
 */
static size_t get_max_line_width(PLine *lines, size_t line_count) {
    size_t max_line_width = 0;
    for (size_t i = 0; i < line_count; i++)
        if (lines[i].line_width > max_line_width) {
            max_line_width = lines[i].line_width;
        }
    return max_line_width;
}

/** Computes the panel inner width (chars between the vertical border chars). */
static int compute_inner_width(ScPanelOpts opts, size_t max_cw, ScSpacing sp) {
    if (opts.full_width) {
        int w = sc_terminal_width() - 2 - sp.margin.left - sp.margin.right;
        return w < 2 ? 2 : w;
    }
    if (opts.width > 0)
        return opts.width - 2;
    int title_len = opts.title.text
        ? (int)sc_utf8_string_length(opts.title.text, strlen(opts.title.text))
        : 0;
    int min4title = opts.title.text ? title_len + 2 * opts.title.pad + 2 : 0;
    int from_content = (int)max_cw + sp.padding.left + sp.padding.right;
    int w = from_content > min4title ? from_content : min4title;
    return w < 2 ? 2 : w;
}

/** Constructs an `HBorder` from a border style, inner width, and corner characters. */
static HBorder make_hborder(ScBorderStyle style, int inner_w, const char *l, const char *r) {
    return (HBorder){
        .border_style         = style,
        .inner_width          = inner_w,
        .left_edge_character  = (char *)l,
        .right_edge_character = (char *)r,
    };
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

static void add_vertical_margin(int line_count) {
    for (int i = 0; i < line_count; i++) {
        fputc('\n', stdout);
    }
}

/** Prints the left margin then renders `hborder` with `title` visible only at `pos`. */
static void render_panel_border(HBorder hborder, ScTitle title, ScTitlePosition pos, int ml) {
    print_margin(ml);
    if (title.pos != pos) title.text = NULL;
    render_horizontal_border(hborder, title);
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

/** Renders top padding rows, all content lines and bottom padding rows. */
static void render_body(PLineView row, PLine *lines, size_t count, ScSpacing sp) {
    for (int i = 0; i < sp.padding.top; i++) { print_margin(sp.margin.left); render_empty_line(row); }
    for (size_t i = 0; i < count; i++) {
        row.line = &lines[i];
        print_margin(sp.margin.left);
        render_content_line(row);
    }
    for (int i = 0; i < sp.padding.bottom; i++) { print_margin(sp.margin.left); render_empty_line(row); }
}

/** Frees the span strings and line array produced by `split_text_into_panel_lines`. */
static void free_plines(PLine *lines, size_t n) {
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < lines[i].count; j++)
            free((char *)lines[i].spans[j].text);
        free(lines[i].spans);
    }
    free(lines);
}

