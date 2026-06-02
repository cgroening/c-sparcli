#include "sparcli.h"
#include "core/text_internal.h"
#include "internal.h"
#include "core/render_wrap.h"
#include <string.h>
#include <stdlib.h>


/**
 * Combines padding and margin insets for layout. Both are `ScEdges`
 * (top/right/bottom/left); zero-initialization means no padding/margin.
 */
typedef struct ScSpacing {
    ScEdges padding;
    ScEdges margin;
} ScSpacing;

/**
 * Bundles the arguments passed to `render_horizontal_border`.
 */
typedef struct HBorder {
    /** Border style (type, color and bg) */
    ScBorderStyle border_style;

    /** Number of chars between the edge chars. */
    int           inner_width;

    /** Left corner character */
    const char   *left_edge_character;

    /** Right corner character */
    const char   *right_edge_character;
} HBorder;

/**
 * Layout parameters for a content line within the panel interior.
 */
typedef struct PLineLayout {
    /** Chars between the vertical edge characters */
    int      inner_width;

    /** Left content padding in columns */
    int      pad_left;

    /** Right content padding in columns */
    int      pad_right;

    /** Horizontal alignment of the content */
    ScHAlign content_align;
} PLineLayout;

/**
 * All parameters needed to render one panel row (content or empty).
 */
typedef struct PLineView {
    /** Interior width, padding, and alignment */
    PLineLayout   layout;

    /** Border style for the vertical edge characters */
    ScBorderStyle border_style;

    /** Content area background color, excluding border */
    ScColor       content_bg;
} PLineView;


/**
 * Internal rendering context for a panel.
 *
 * Holds both the caller-supplied inputs and all derived layout values so that
 * helper functions only need a single `Panel *` argument.
 */
typedef struct Panel {
    /** Content to render inside the panel (not owned) */
    const ScText *text;

    /** Layout and visual options (border, title, width, …) */
    ScPanelOpts   opts;

    /** Parsed content lines split from `text` on `\n` boundaries (owned) */
    ScRenderLine        *lines;

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

/**
 * Accumulator used while splitting `ScText` spans into `ScRenderLine`s.
 */
typedef struct ParseBuf {
    /** Growing span buffer for the current line */
    ScRenderSpan       *spans;

    /** Spans written into `spans` */
    size_t       span_count;

    /** Allocated capacity of `spans` */
    size_t       span_capacity;

    /** Visible column width accumulated for the current line */
    size_t       span_width;

    /** Style of the source span currently being scanned */
    ScTextStyle  style;

    /** Growing array of completed lines */
    ScRenderLine       *lines;

    /** Lines written into `lines` */
    size_t       line_count;

    /** Allocated capacity of `lines` */
    size_t       line_capacity;
} ParseBuf;


// Forward declarations indented to reflect call hierarchy
static void panel_init(Panel *panel, const ScText *text, ScPanelOpts opts);
    static void split_text_into_panel_lines(Panel *panel);
        static void scan_source_span(ParseBuf *buf, const ScSpan *span);
            static void append_span(
                ParseBuf *buf, const char *start, size_t length
            );
            static void flush_line(ParseBuf *buf);
        static void finish_parse_buf(Panel *panel, ParseBuf *buf);
    static void resolve_panel_spacing(Panel *panel);
    static void compute_max_line_width(Panel *panel);
    static void compute_inner_width(Panel *panel);
        static int panel_title_min_width(const Panel *panel);
    static void wrap_panel_content(Panel *panel);
    static void create_line_view_template(Panel *panel);
    static void resolve_horizontal_border_lines(Panel *panel);
        static HBorder make_hborder(Panel *panel, ScPosition position);

static void panel_render(Panel *panel);
    static void add_vertical_margin(int n);
    static void render_panel_border(Panel *panel, ScPosition pos);
        static void render_horizontal_border(HBorder hborder, ScTitle title);
            static void print_colored(const char *s, ScBorderStyle style);
                static ScColor norm_bg(ScColor c);
        static void print_repeat(const char *s, int n, ScBorderStyle style);
    static void render_body(Panel *panel);
        static void render_empty_line(Panel *panel);
        static void render_content_line(Panel *panel, ScRenderLine *line);
            static void print_line_spans(ScRenderLine *line, ScColor bg);

static void panel_cleanup(Panel *panel);
    static void free_lines(ScRenderLine *lines, size_t n);


/**
 * Maps each `ScBorderType` to its six box-drawing characters.
 *
 * `tl`/`tr`/`bl`/`br` = corners; `h` = horizontal char; `v` = vertical char.
 */
static const struct {
    const char *tl, *tr, *bl, *br;
    const char *h, *v;
} border_table[] = {
    [SC_BORDER_NONE]    = { " ", " ", " ", " ", " ", " " },
    [SC_BORDER_ASCII]   = { "+", "+", "+", "+", "-", "|" },
    [SC_BORDER_SINGLE]  = { "┌", "┐", "└", "┘", "─", "│" },
    [SC_BORDER_DOUBLE]  = { "╔", "╗", "╚", "╝", "═", "║" },
    [SC_BORDER_ROUNDED] = { "╭", "╮", "╰", "╯", "─", "│" },
    [SC_BORDER_THICK]   = { "┏", "┓", "┗", "┛", "━", "┃" },
};


void sc_panel_text(const ScText *text, ScPanelOpts opts) {
    if (!text) { return; }

    // Title strings are borrowed user strings; sanitize per the panel's
    // ANSI mode (rich_text titles were sanitized at append time already).
    char *clean_title    = sc_sanitize_copy_mode(opts.title.text, opts.ansi);
    char *clean_subtitle =
        sc_sanitize_copy_mode(opts.subtitle.text, opts.ansi);
    opts.title.text    = clean_title;
    opts.subtitle.text = clean_subtitle;

    Panel panel;
    panel_init(&panel, text, opts);
    panel_render(&panel);
    panel_cleanup(&panel);

    free(clean_title);
    free(clean_subtitle);
}

void sc_panel_str(const char *raw_str, ScPanelOpts opts) {
    if (!raw_str) { return; }
    static const ScTextStyle plain = {
        SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
    };

    // Content crosses the trust boundary here, honoring opts.ansi
    char *clean = sc_sanitize_copy_mode(raw_str, opts.ansi);
    if (!clean) { return; }
    ScText *t = sc_text_new();
    sc_text_append_raw(t, clean, plain);
    free(clean);

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
    wrap_panel_content(panel);
    create_line_view_template(panel);
    resolve_horizontal_border_lines(panel);
}

/**
 * Splits the spans of `text` on newline boundaries into an array of
 * `ScRenderLine` records. Each `ScRenderLine` owns heap copies of its span
 * strings.
 * Caller must free the result with `free_lines`.
 */
static void split_text_into_panel_lines(Panel *panel) {
    ParseBuf buf = {
        .spans = malloc(SC_INITIAL_CAPACITY * sizeof(ScRenderSpan)),
        .span_capacity = SC_INITIAL_CAPACITY,
        .lines = malloc(SC_INITIAL_CAPACITY * sizeof(ScRenderLine)),
        .line_capacity = SC_INITIAL_CAPACITY,
    };
    if (!buf.spans || !buf.lines) {
        free(buf.spans);
        free(buf.lines);
        panel->lines = NULL;
        panel->line_count = 0;
        return;
    }

    for (size_t si = 0; si < panel->text->count; si++) {
        scan_source_span(&buf, &panel->text->spans[si]);
    }

    finish_parse_buf(panel, &buf);
}

/**
 * Scans one source span for `\n`, flushing a `ScRenderLine` on each new line
 * and buffering the rest.
 */
static void scan_source_span(ParseBuf *buf, const ScSpan *span) {
    buf->style          = span->style;
    const char *cursor = span->raw_str;
    const char *start  = cursor;

    while (*cursor) {
        if (*cursor == '\n') {
            size_t length = (size_t)(cursor - start);
            if (length > 0) { append_span(buf, start, length); }
            flush_line(buf);
            start = cursor + 1;
        }
        cursor++;
    }
    size_t length = (size_t)(cursor - start);
    if (length > 0) { append_span(buf, start, length); }
}

/**
 * Grows the span buffer if full, then appends a heap copy of [start, length).
 */
static void append_span(ParseBuf *buf, const char *start, size_t length) {
    if (buf->span_count == buf->span_capacity) {
        void *grown = sc_dynarray_grow(
            buf->spans, &buf->span_capacity,
            sizeof(ScRenderSpan), SC_INITIAL_CAPACITY
        );
        if (!grown) { return; }
        buf->spans = grown;
    }
    char *copy = strndup(start, length);
    if (!copy) { return; }   // skip the span rather than store NULL text
    buf->spans[buf->span_count++] = (ScRenderSpan){ copy, buf->style };
    buf->span_width += sc_utf8_string_length(start, length);
}

/**
 * Copies the span buffer into a new `ScRenderLine`, appends it to the line
 * array and resets the span buffer.
 */
static void flush_line(ParseBuf *buf) {
    if (buf->line_count == buf->line_capacity) {
        void *grown = sc_dynarray_grow(
            buf->lines, &buf->line_capacity,
            sizeof(ScRenderLine), SC_INITIAL_CAPACITY
        );
        if (!grown) { goto drop; }
        buf->lines = grown;
    }
    ScRenderSpan *ls = malloc((buf->span_count + 1) * sizeof(ScRenderSpan));
    if (!ls) { goto drop; }
    memcpy(ls, buf->spans, buf->span_count * sizeof(ScRenderSpan));
    buf->lines[buf->line_count++] =
        (ScRenderLine){ ls, buf->span_count, buf->span_width };
    buf->span_count = 0;
    buf->span_width = 0;
    return;

drop:
    // OOM: drop the pending line, freeing its span strings so they don't leak.
    for (size_t i = 0; i < buf->span_count; i++) { free(buf->spans[i].text); }
    buf->span_count = 0;
    buf->span_width = 0;
}

/**
 * Flushes the last line, transfers ownership of the line array to `panel`
 * and frees the span buffer.
 */
static void finish_parse_buf(Panel *panel, ParseBuf *buf) {
    flush_line(buf);
    free(buf->spans);
    panel->lines      = buf->lines;
    panel->line_count = buf->line_count;
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

/**
 * Sets `panel->max_line_width` to the visible column width of the widest line.
 */
static void compute_max_line_width(Panel *panel) {
    size_t max = 0;
    for (size_t i = 0; i < panel->line_count; i++) {
        if (panel->lines[i].visible_width > max) {
            max = panel->lines[i].visible_width;
        }
    }
    panel->max_line_width = max;
}

/**
 * Sets `panel->inner_width` to the number of chars between the vertical border
 * chars.
 */
static void compute_inner_width(Panel *panel) {
    ScPanelOpts opts = panel->opts;
    ScSpacing   sp   = panel->spacing;

    int w;
    if (opts.full_width) {
        w = sc_terminal_width() - 2 - sp.margin.left - sp.margin.right;
    } else if (opts.width > 0) {
        w = opts.width - 2;
    } else {
        int title_min_width = panel_title_min_width(panel);
        int from_content = (int)panel->max_line_width
            + sp.padding.left + sp.padding.right;
        int natural = from_content > title_min_width
            ? from_content : title_min_width;
        // Never let an auto-sized panel grow past the terminal; content is
        // wrapped to fit by wrap_panel_content afterwards.
        int cap = sc_terminal_width() - 2 - sp.margin.left - sp.margin.right;
        w = natural < cap ? natural : cap;
    }
    panel->inner_width = w < 2 ? 2 : w;
}

/**
 * Minimum inner width required to display one title inline (title length +
 * padding on both sides + one dash per corner), or 0 when `title` has no text.
 */
static int one_title_min_width(ScTitle title) {
    if (!title.text && !title.rich_text) { return 0; }
    int len = title.rich_text
        ? (int)sc_text_visible_width(title.rich_text)
        : (int)sc_utf8_string_length(title.text, strlen(title.text));
    int pad = title.pad > 0 ? title.pad : 0;
    return len + 2 * pad + 2;
}

/**
 * Returns the minimum inner width needed for both the title and the optional
 * subtitle (they may sit on different edges, so the panel must fit the wider
 * of the two), or 0 when the panel has no titles.
 */
static int panel_title_min_width(const Panel *panel) {
    int a = one_title_min_width(panel->opts.title);
    int b = one_title_min_width(panel->opts.subtitle);
    return a > b ? a : b;
}

/**
 * Word-wraps content lines that are wider than the interior content area so
 * they never overflow the border. When the panel is auto-sized, the frame is
 * then shrunk back to the widest wrapped line (it is never grown past the
 * target computed by `compute_inner_width`).
 */
static void wrap_panel_content(Panel *panel) {
    int content_width = panel->inner_width
        - panel->spacing.padding.left - panel->spacing.padding.right;
    if (content_width < 1) { content_width = 1; }

    bool any_over = false;
    for (size_t i = 0; i < panel->line_count; i++) {
        if ((int)panel->lines[i].visible_width > content_width) {
            any_over = true;
            break;
        }
    }
    if (!any_over) { return; }

    size_t new_count;
    ScRenderLine *wrapped = sc_wrap_render_lines(
        panel->lines, panel->line_count, content_width, &new_count
    );
    free_lines(panel->lines, panel->line_count);
    panel->lines = wrapped;
    panel->line_count = new_count;
    compute_max_line_width(panel);

    // Auto-sized panels snap the frame to the wrapped content width.
    if (!panel->opts.full_width && panel->opts.width <= 0) {
        int title_min_width = panel_title_min_width(panel);
        int from_content = (int)panel->max_line_width
            + panel->spacing.padding.left + panel->spacing.padding.right;
        int snug = from_content > title_min_width
            ? from_content : title_min_width;
        if (snug < panel->inner_width) { panel->inner_width = snug; }
        if (panel->inner_width < 2)    { panel->inner_width = 2; }
    }
}

/** Builds the `PLineView` template shared by all content and empty rows. */
static void create_line_view_template(Panel *panel) {
    panel->line_view_template = (PLineView){
        .layout = {
            .inner_width = panel->inner_width,
            .pad_left = panel->spacing.padding.left,
            .pad_right = panel->spacing.padding.right,
            .content_align = panel->opts.content_align
        },
        .border_style = panel->opts.border,
        .content_bg     = panel->opts.bg,
    };
}

/** Constructs and stores the top and bottom `HBorder` records in `panel`. */
static void resolve_horizontal_border_lines(Panel *panel) {
    panel->top_border    = make_hborder(panel, SC_POSITION_TOP);
    panel->bottom_border = make_hborder(panel, SC_POSITION_BOTTOM);
}


/**
 * Constructs an `HBorder` for `position`, selecting corner characters
 * from `border_table`.
 */
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


/** Renders the panel to stdout. */
static void panel_render(Panel *panel) {
    add_vertical_margin(panel->spacing.margin.top);
    render_panel_border(panel, SC_POSITION_TOP);
    render_body(panel);
    render_panel_border(panel, SC_POSITION_BOTTOM);
    add_vertical_margin(panel->spacing.margin.bottom);
}

/** Emits `count` blank lines to stdout. */
static void add_vertical_margin(int count) {
    for (int i = 0; i < count; i++) {
        fputc('\n', sc_output_stream());
    }
}

/**
 * Prints the left margin then renders the top or bottom border with its title.
 */
static void render_panel_border(Panel *panel, ScPosition pos) {
    HBorder hborder = (pos == SC_POSITION_TOP)
        ? panel->top_border : panel->bottom_border;
    sc_print_spaces(panel->spacing.margin.left);
    /* Pick whichever title belongs on this edge: primary first, else the
     * optional subtitle, else none. A title exists when either its plain
     * text or its rich text is set. */
    ScTitle title = { 0 };
    bool has_title = panel->opts.title.text || panel->opts.title.rich_text;
    bool has_subtitle =
        panel->opts.subtitle.text || panel->opts.subtitle.rich_text;
    if (has_title && panel->opts.title.pos == pos) {
        title = panel->opts.title;
    } else if (has_subtitle && panel->opts.subtitle.pos == pos) {
        title = panel->opts.subtitle;
    }
    render_horizontal_border(hborder, title);
}

/**
 * Renders a horizontal border line with an optional inline title.
 *
 * Distributes remaining dashes around the title per `title.halign`,
 * guaranteeing at least one dash on each side. Pad spaces are printed
 * using the title style's own background color.
 */
static void render_horizontal_border(HBorder hborder, ScTitle title) {
    const char *h = border_table[hborder.border_style.type].h;
    if (title.pad < 0) { title.pad = 0; }
    ScBorderStyle title_pad_style = { 0, SC_ANSI_COLOR_NONE, title.style.bg };
    print_colored(hborder.left_edge_character, hborder.border_style);

    bool has_title = title.rich_text || (title.text && *title.text);
    if (has_title) {
        int tlen   = title.rich_text
            ? (int)sc_text_visible_width(title.rich_text)
            : (int)sc_utf8_string_length(title.text, strlen(title.text));
        int dashes = hborder.inner_width - tlen - 2 * title.pad;
        if (dashes < 0) { dashes = 0; }

        int left_dashes, right_dashes;
        if (title.halign == SC_ALIGN_LEFT) {
            left_dashes = 1; right_dashes = dashes - 1;
        } else if (title.halign == SC_ALIGN_RIGHT) {
            left_dashes = dashes - 1; right_dashes = 1;
        } else {  // CENTER
            left_dashes = dashes / 2; right_dashes = dashes - left_dashes;
        }
        if (left_dashes < 0)  { left_dashes = 0; }
        if (right_dashes < 0) { right_dashes = 0; }

        print_repeat(h, left_dashes, hborder.border_style);
        for (int i = 0; i < title.pad; i++) {
            print_colored(" ", title_pad_style);
        }
        if (title.rich_text) {
            sc_print_text(title.rich_text);
        } else {
            sc_print_raw(title.text, title.style);
        }
        for (int i = 0; i < title.pad; i++) {
            print_colored(" ", title_pad_style);
        }
        print_repeat(h, right_dashes, hborder.border_style);
    } else {
        print_repeat(h, hborder.inner_width, hborder.border_style);
    }

    print_colored(hborder.right_edge_character, hborder.border_style);
    fputc('\n', sc_output_stream());
}

/**
 * Applies `style.color`/`style.bg`, prints `s`, then emits a reset.
 */
static void print_colored(const char *s, ScBorderStyle style) {
    sc_apply_colors(style.color, norm_bg(style.bg));
    fputs(s, sc_output_stream());
    fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());
}

/** Returns `SC_ANSI_COLOR_NONE` for the zero-init sentinel; else `color`. */
static ScColor norm_bg(ScColor color) {
    return sc_color_is_active(color) ? color : SC_ANSI_COLOR_NONE;
}

/**
 * Returns true if `color` should cause ANSI color escape codes to be emitted.
 *
 * Returns false for `SC_ANSI_COLOR_NONE` (`index == 0`, which is also the
 * zero-init value), true for any named color (`index >= 1`) or RGB mode
 * (`index == -1`).
 */

/**
 * Applies `style.color`/`style.bg`, prints `str_raw` `count` times,
 * then emits a reset.
 */
static void print_repeat(const char *str_raw, int count, ScBorderStyle style) {
    if (count <= 0) { return; }
    sc_apply_colors(style.color, norm_bg(style.bg));
    for (int i = 0; i < count; i++) { fputs(str_raw, sc_output_stream()); }
    fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());
}

/** Renders top padding rows, all content lines, and bottom padding rows. */
static void render_body(Panel *panel) {
    ScSpacing sp = panel->spacing;

    for (int i = 0; i < sp.padding.top; i++) {
        sc_print_spaces(sp.margin.left);
        render_empty_line(panel);
    }

    for (size_t i = 0; i < panel->line_count; i++) {
        sc_print_spaces(sp.margin.left);
        render_content_line(panel, &panel->lines[i]);
    }

    for (int i = 0; i < sp.padding.bottom; i++) {
        sc_print_spaces(sp.margin.left);
        render_empty_line(panel);
    }
}

/**
 * Renders one border-enclosed blank row, filling the interior with `bg` color
 * if active.
 */
static void render_empty_line(Panel *panel) {
    PLineView row = panel->line_view_template;
    print_colored(border_table[row.border_style.type].v, row.border_style);
    if (sc_color_is_active(row.content_bg)) {
        sc_apply_colors(SC_ANSI_COLOR_NONE, row.content_bg);
        for (int i = 0; i < row.layout.inner_width; i++) {
            fputc(' ', sc_output_stream());
        }
        fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());
    } else {
        for (int i = 0; i < row.layout.inner_width; i++) {
            fputc(' ', sc_output_stream());
        }
    }
    print_colored(border_table[row.border_style.type].v, row.border_style);
    fputc('\n', sc_output_stream());
}

/**
 * Renders one content row with left/right padding and alignment spacing.
 *
 * Re-applies bg after each `sc_print` call because `sc_print` always emits
 * a reset that would otherwise clear the background color.
 */
static void render_content_line(Panel *panel, ScRenderLine *line) {
    PLineView row = panel->line_view_template;
    PLineLayout layout = row.layout;

    // Alignment
    int content_space = layout.inner_width - layout.pad_left - layout.pad_right
                        - (int)line->visible_width;
    if (content_space < 0) { content_space = 0; }
    int left_padding = 0, right_padding = content_space;
    if (layout.content_align == SC_ALIGN_CENTER) {
        left_padding = content_space / 2;
        right_padding = content_space - left_padding;
    }
    else if (layout.content_align == SC_ALIGN_RIGHT)  {
        left_padding = content_space;
        right_padding = 0;
    }

    // Output
    print_colored(border_table[row.border_style.type].v, row.border_style);
    if (sc_color_is_active(row.content_bg)) {
        sc_apply_colors(SC_ANSI_COLOR_NONE, row.content_bg);
    }
    sc_print_spaces(layout.pad_left);
    sc_print_spaces(left_padding);
    print_line_spans(line, row.content_bg);
    sc_print_spaces(right_padding);
    sc_print_spaces(layout.pad_right);
    if (sc_color_is_active(row.content_bg)) {
        fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());
    }
    print_colored(border_table[row.border_style.type].v, row.border_style);
    fputc('\n', sc_output_stream());
}

/** Prints each span of `line`, re-applying `bg` after each reset if active. */
static void print_line_spans(ScRenderLine *line, ScColor bg) {
    for (size_t i = 0; i < line->count; i++) {
        sc_print_raw(line->spans[i].text, line->spans[i].style);
        if (sc_color_is_active(bg)) { sc_apply_colors(SC_ANSI_COLOR_NONE, bg); }
    }
}


/** Frees heap-allocated panel content. */
static void panel_cleanup(Panel *panel) {
    free_lines(panel->lines, panel->line_count);
}

/**
 * Frees the span strings and line array produced by
 * `split_text_into_panel_lines`.
 */
static void free_lines(ScRenderLine *lines, size_t count) {
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < lines[i].count; j++) {
            free(lines[i].spans[j].text);
        }
        free(lines[i].spans);
    }
    free(lines);
}
