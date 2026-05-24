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
    const char   *left_edge_character;   /**< Left corner character */
    const char   *right_edge_character;  /**< Right corner character */
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
    int      inner_width;    /**< Chars between the vertical edge characters */
    int      pad_l;          /**< Left content padding in columns */
    int      pad_r;          /**< Right content padding in columns */
    ScHAlign content_align;  /**< Horizontal alignment of the content */
} PLineLayout;

/**
 * PLineView – All parameters needed to render one panel row (content or empty).
 */
typedef struct {
    /** Interior width, padding, and alignment */
    PLineLayout   layout;

    /** Border style for the vertical edge characters */
    ScBorderStyle border_style;

    /** Content area background color, excluding borders */
    ScColor       content_bg;
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


static void panel_init(Panel *panel, const ScText *text, ScPanelOpts opts);
    static void split_text_into_panel_lines(Panel *panel);
    static void resolve_panel_spacing(Panel *panel);
    static void compute_max_line_width(Panel *panel);
    static void compute_inner_width(Panel *panel);
    static void create_line_view_template(Panel *panel);
    static void resolve_horizontal_border_lines(Panel *panel);
        static HBorder make_hborder(Panel *panel, ScPosition position);

static void panel_render(Panel *panel);
    static void add_vertical_margin(int n);
    static void render_panel_border(Panel *panel, ScPosition pos);
        static void print_margin(int n);
        static void render_horizontal_border(HBorder hborder, ScTitle title);
            static void print_colored(const char *s, ScBorderStyle style);
                static ScColor norm_bg(ScColor c);
                    static bool is_color_active(ScColor c);
        static void print_repeat(const char *s, int n, ScBorderStyle style);
    static void render_body(Panel *panel);
        static void render_content_line(Panel *panel, PLine *line);
        static void render_empty_line(Panel *panel);

static void panel_cleanup(Panel *panel);
    static void free_lines(PLine *lines, size_t n);






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
    Panel panel;
    panel_init(&panel, text, opts);
    panel_render(&panel);
    panel_cleanup(&panel);
}

void sc_panel_str(const char *raw_str, ScPanelOpts opts) {
    ScText *t = sc_text_from_str(raw_str);
    sc_panel_text(t, opts);
    sc_text_free(t);
}

/** Populates all `Panel` fields from `text` and `opts`. */
static void panel_init(Panel *panel, const ScText *text, ScPanelOpts opts) {
    panel->text = text;
    panel->opts = opts;
    split_text_into_panel_lines(panel);
    resolve_panel_spacing(panel);
    compute_max_line_width(panel);
    compute_inner_width(panel);
    create_line_view_template(panel);
    resolve_horizontal_border_lines(panel);
}

/** Renders the panel to stdout. */
static void panel_render(Panel *panel) {
    add_vertical_margin(panel->spacing.margin.top);
    render_panel_border(panel, SC_POSITION_TOP);
    render_body(panel);
    render_panel_border(panel, SC_POSITION_BOTTOM);
    add_vertical_margin(panel->spacing.margin.bottom);
}

/** Frees heap-allocated panel content. */
static void panel_cleanup(Panel *panel) {
    free_lines(panel->lines, panel->line_count);
}

/**
 * Splits the spans of `text` on newline boundaries into an array of `PLine`
 * records. Each `PLine` owns heap copies of its span strings.
 * Caller must free the result with `free_lines`.
 */
static void split_text_into_panel_lines(Panel *panel) {
    size_t lines_cap = 8, nlines = 0;
    PLine *lines = malloc(lines_cap * sizeof(PLine));

    size_t buf_cap = 8, buf_n = 0, buf_w = 0;
    PSpan *buf = malloc(buf_cap * sizeof(PSpan));

    for (size_t si = 0; si < panel->text->count; si++) {
        const char *s     = panel->text->spans[si].raw_str;
        ScTextStyle   opts  = panel->text->spans[si].style;
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
    panel->lines      = lines;
    panel->line_count = nlines;
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
static bool is_color_active(ScColor color) {
    bool is_none     = color.index == -2;
    bool is_zeroinit = color.index == 0 && !color.r && !color.g && !color.b;
    return !is_none && !is_zeroinit;
}

/** Returns `SC_ANSI_COLOR_NONE` for the zero-init sentinel; else `color`. */
static ScColor norm_bg(ScColor color) {
    return is_color_active(color) ? color : SC_ANSI_COLOR_NONE;
}

/** Applies `style.color`/`style.bg`, prints `s`, then emits a reset. */
static void print_colored(const char *s, ScBorderStyle style) {
    sc_apply_colors(style.color, norm_bg(style.bg));
    fputs(s, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

/** Applies `style.color`/`style.bg`, prints `s` `n` times, then emits a reset. */
static void print_repeat(const char *str_raw, int count, ScBorderStyle style) {
    if (count <= 0) return;
    sc_apply_colors(style.color, norm_bg(style.bg));
    for (int i = 0; i < count; i++) fputs(str_raw, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

/** Clamps all padding and margin values from `panel->opts` to >= 0. */
static void resolve_panel_spacing(Panel *panel) {
    ScPanelOpts opts = panel->opts;
    panel->spacing = (ScSpacing){
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

/** Sets `panel->max_line_width` to the visible column width of the widest line. */
static void compute_max_line_width(Panel *panel) {
    size_t max = 0;
    for (size_t i = 0; i < panel->line_count; i++)
        if (panel->lines[i].line_width > max)
            max = panel->lines[i].line_width;
    panel->max_line_width = max;
}

/** Sets `panel->inner_width` to the number of chars between the vertical border chars. */
static void compute_inner_width(Panel *panel) {
    ScPanelOpts opts = panel->opts;
    ScSpacing   sp   = panel->spacing;
    int w;
    if (opts.full_width) {
        w = sc_terminal_width() - 2 - sp.margin.left - sp.margin.right;
    } else if (opts.width > 0) {
        w = opts.width - 2;
    } else {
        int title_len = opts.title.text
            ? (int)sc_utf8_string_length(opts.title.text, strlen(opts.title.text))
            : 0;
        int min4title = opts.title.text ? title_len + 2 * opts.title.pad + 2 : 0;
        int from_content = (int)panel->max_line_width + sp.padding.left + sp.padding.right;
        w = from_content > min4title ? from_content : min4title;
    }
    panel->inner_width = w < 2 ? 2 : w;
}


static void create_line_view_template(Panel *panel) {
    panel->line_view_template = (PLineView){
        .layout = {
            .inner_width = panel->inner_width,
            .pad_l = panel->spacing.padding.left,
            .pad_r = panel->spacing.padding.right,
            .content_align = panel->opts.content_align
        },
        .border_style = panel->opts.border,
        .content_bg     = panel->opts.bg,
    };
}


static void resolve_horizontal_border_lines(Panel *panel) {

    panel->top_border    = make_hborder(panel, SC_POSITION_TOP);
    panel->bottom_border = make_hborder(panel, SC_POSITION_BOTTOM);

}



/**  */
static HBorder make_hborder(Panel *panel, ScPosition position) {
    int border_type = panel->opts.border.type;
    const char *left_edge_character;
    const char *right_edge_character;
    if (position == SC_POSITION_TOP) {
        left_edge_character  = border_table[border_type].tl;
        right_edge_character = border_table[border_type].tr;
    } else {
        left_edge_character  = border_table[border_type].bl;
        right_edge_character = border_table[border_type].br;
    }

    return (HBorder){
        .border_style         = panel->opts.border,
        .inner_width          = panel->inner_width,
        .left_edge_character  = left_edge_character,
        .right_edge_character = right_edge_character,
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

/** Prints the left margin then renders the top or bottom border with its title. */
static void render_panel_border(Panel *panel, ScPosition pos) {
    HBorder hborder = (pos == SC_POSITION_TOP) ? panel->top_border : panel->bottom_border;
    print_margin(panel->spacing.margin.left);
    ScTitle title = panel->opts.title;
    if (title.pos != pos) title.text = NULL;
    render_horizontal_border(hborder, title);
}

/** Renders one border-enclosed blank row, filling the interior with bg if active. */
static void render_empty_line(Panel *panel) {
    PLineView row = panel->line_view_template;
    print_colored(border_table[row.border_style.type].v, row.border_style);
    if (is_color_active(row.content_bg)) {
        sc_apply_colors(SC_ANSI_COLOR_NONE, row.content_bg);
        for (int i = 0; i < row.layout.inner_width; i++) fputc(' ', stdout);
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    } else {
        for (int i = 0; i < row.layout.inner_width; i++) fputc(' ', stdout);
    }
    print_colored(border_table[row.border_style.type].v, row.border_style);
    fputc('\n', stdout);
}

/**
 * Renders one content row with left/right padding and alignment spacing.
 *
 * Re-applies bg after each `sc_print` call because `sc_print` always emits
 * a reset that would otherwise clear the background color.
 */
static void render_content_line(Panel *panel, PLine *line) {
    PLineView   row = panel->line_view_template;
    PLineLayout l   = row.layout;
    int spare = l.inner_width - l.pad_l - l.pad_r - (int)line->line_width;
    if (spare < 0) spare = 0;
    int lp = 0, rp = spare;
    if (l.content_align == SC_ALIGN_CENTER) { lp = spare / 2; rp = spare - lp; }
    else if (l.content_align == SC_ALIGN_RIGHT) { lp = spare; rp = 0; }

    print_colored(border_table[row.border_style.type].v, row.border_style);
    if (is_color_active(row.content_bg))
        sc_apply_colors(SC_ANSI_COLOR_NONE, row.content_bg);
    for (int i = 0; i < l.pad_l; i++) fputc(' ', stdout);
    for (int i = 0; i < lp; i++) fputc(' ', stdout);
    for (size_t i = 0; i < line->count; i++) {
        sc_print(line->spans[i].text, line->spans[i].opts);
        if (is_color_active(row.content_bg))
            sc_apply_colors(SC_ANSI_COLOR_NONE, row.content_bg);
    }
    for (int i = 0; i < rp; i++) fputc(' ', stdout);
    for (int i = 0; i < l.pad_r; i++) fputc(' ', stdout);
    if (is_color_active(row.content_bg))
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    print_colored(border_table[row.border_style.type].v, row.border_style);
    fputc('\n', stdout);
}

/** Renders top padding rows, all content lines, and bottom padding rows. */
static void render_body(Panel *panel) {
    ScSpacing sp = panel->spacing;
    for (int i = 0; i < sp.padding.top; i++) { print_margin(sp.margin.left); render_empty_line(panel); }
    for (size_t i = 0; i < panel->line_count; i++) {
        print_margin(sp.margin.left);
        render_content_line(panel, &panel->lines[i]);
    }
    for (int i = 0; i < sp.padding.bottom; i++) { print_margin(sp.margin.left); render_empty_line(panel); }
}

/** Frees the span strings and line array produced by `split_text_into_panel_lines`. */
static void free_lines(PLine *lines, size_t n) {
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < lines[i].count; j++)
            free((char *)lines[i].spans[j].text);
        free(lines[i].spans);
    }
    free(lines);
}

