#include "sparcli.h"
#include "internal.h"
#include "table/table_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Default title padding when `opts.title.pad <= 0`. */
#define DEFAULT_TITLE_PAD 1


// Forward declarations indented to reflect call hierarchy
static void adjust_corner_chars_for_rowspan(
    const Table *table, const bool *rowspan_flags,
    const char **left_corner, const char **right_corner
);
static void render_border_column_fill(
    const Table *table, size_t col,
    const bool *rowspan_flags, HBorderSpec spec
);
static void render_border_column_junction(
    const Table *table, size_t col,
    const bool *rowspan_flags, HBorderSpec spec
);
    static const char *select_junction_char(
        const Table *table, size_t col,
        const bool *rowspan_flags, HBorderSpec spec
    );
    static ScColor select_junction_color(
        const Table *table, size_t col, HBorderSpec spec
    );

static void render_inner_sep_span_column(Table *table, size_t col);
    static void render_inner_sep_span_content(
        Table *table, size_t col, ScColor col_bg
    );
        static int rowspan_separator_line_index(
            const RowSpan *span, int content_lines
        );
    static void render_inner_sep_fill(Table *table, size_t col);
    static void render_inner_sep_junction(Table *table, size_t col);

static void render_title_inner(
    const Table *table, const char *fill_char,
    ScColor outer_color, int title_pad
);
    static void render_title_with_fill(
        const Table *table, const char *fill_char,
        ScColor outer_color, int title_pad
    );
        static void split_title_fill(
            int inner_width, int title_length, int title_pad,
            ScHAlign align, int *left_fill, int *right_fill
        );
    static void render_title_plain_fill(
        int inner_width, const char *fill_char, ScColor outer_color
    );


/* ── render_horizontal_border ───────────────────────────────────────────── */

/**
 * Renders one horizontal border line (top, bottom, or inner separator).
 *
 * Adjusts edge corners for active rowspans, then prints margin, left edge,
 * per-column fill and junction characters, right edge, and a newline.
 * `rowspan_flags[col]` is `true` when `col` is inside an active rowspan.
 */
void render_horizontal_border(
    const Table *table, HBorderSpec spec, const bool *rowspan_flags
) {
    const ScTableData *data = table->table_data;
    bool right_to_left = table->opts.right_to_left;
    bool no_outer = table->opts.border.no_outer;
    const char *left_corner = spec.left_corner_char;
    const char *right_corner = spec.right_corner_char;

    if (rowspan_flags && !no_outer) {
        adjust_corner_chars_for_rowspan(
            table, rowspan_flags, &left_corner, &right_corner
        );
    }

    print_left_margin(table);
    if (!no_outer) { print_colored_string(left_corner, spec.edge_color); }

    for (size_t i = 0; i < data->column_count; i++) {
        size_t col = right_to_left ? (data->column_count - 1 - i) : i;
        render_border_column_fill(table, col, rowspan_flags, spec);
        if (i < data->column_count - 1 && spec.column_separator) {
            render_border_column_junction(table, col, rowspan_flags, spec);
        }
    }

    if (!no_outer) { print_colored_string(right_corner, spec.edge_color); }
    fputc('\n', sc_output_stream());
}

/**
 * Replaces `*left_corner`/`*right_corner` with the vertical-bar glyph when
 * the first or last rendered column has an active rowspan.
 */
static void adjust_corner_chars_for_rowspan(
    const Table *table, const bool *rowspan_flags,
    const char **left_corner, const char **right_corner
) {
    const ScTableData *data = table->table_data;
    ScBorderType style = table->opts.border.type;
    bool right_to_left = table->opts.right_to_left;

    size_t first_col = right_to_left ? (data->column_count - 1) : 0;
    size_t last_col = right_to_left ? 0 : (data->column_count - 1);

    if (rowspan_flags[first_col]) {
        *left_corner = border_char_sets[style].v;
    }
    if (rowspan_flags[last_col]) {
        *right_corner = border_char_sets[style].v;
    }
}

/**
 * Prints the fill segment for `col`: spaces when a rowspan is active in
 * the column, otherwise the repeated fill character in `spec.inner_color`.
 */
static void render_border_column_fill(
    const Table *table, size_t col,
    const bool *rowspan_flags, HBorderSpec spec
) {
    if (rowspan_flags && rowspan_flags[col]) {
        for (int i = 0; i < table->column_widths[col]; i++) {
            fputc(' ', sc_output_stream());
        }
        return;
    }
    sc_apply_colors(spec.inner_color, SC_ANSI_COLOR_NONE);
    for (int i = 0; i < table->column_widths[col]; i++) {
        fputs(spec.fill_char, sc_output_stream());
    }
    fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());
}

/**
 * Prints the junction character between `col` and its neighbor, selecting
 * the glyph based on which side has an active rowspan and applying
 * `header_col_sep_color` when configured and applicable.
 */
static void render_border_column_junction(
    const Table *table, size_t col,
    const bool *rowspan_flags, HBorderSpec spec
) {
    if (!table_has_vertical_separator_after(table, col)) { return; }

    const char *junction_char = select_junction_char(
        table, col, rowspan_flags, spec
    );
    ScColor junction_color = select_junction_color(table, col, spec);
    print_colored_string(junction_char, junction_color);
}

/**
 * Returns the junction character at the boundary after `col`: defaults to
 * `spec.column_separator`, but switches to vertical/T-shaped glyphs when
 * a neighboring column has an active rowspan.
 */
static const char *select_junction_char(
    const Table *table, size_t col,
    const bool *rowspan_flags, HBorderSpec spec
) {
    if (!rowspan_flags) { return spec.column_separator; }

    ScBorderType style = table->opts.border.type;
    bool right_to_left = table->opts.right_to_left;
    size_t neighbor_col = right_to_left ? (col - 1) : (col + 1);
    bool col_in_span = rowspan_flags[col];
    bool neighbor_in_span = rowspan_flags[neighbor_col];

    if (col_in_span && neighbor_in_span) {
        return border_char_sets[style].v;
    }
    if (col_in_span) { return border_char_sets[style].t_left; }
    if (neighbor_in_span) { return border_char_sets[style].t_right; }
    return spec.column_separator;
}

/**
 * Returns the color used for the junction character; overrides to
 * `header_col_sep_color` when the spec opts in and the boundary is at
 * the header-column edge.
 */
static ScColor select_junction_color(
    const Table *table, size_t col, HBorderSpec spec
) {
    bool is_header_edge = table_is_header_column(table, col);
    if (spec.use_header_col_sep_color
        && is_header_edge
        && table->opts.border.header_col_sep_color.index != -2) {
        return table->opts.border.header_col_sep_color;
    }
    return spec.inner_color;
}


/* ── Left margin / colored output ───────────────────────────────────────── */

/** Prints the left margin spaces for the table. */
void print_left_margin(const Table *table) {
    for (int i = 0; i < table->opts.margin.left; i++) { fputc(' ', sc_output_stream()); }
}

/** Prints `str` with foreground color `color`, then emits a reset. */
void print_colored_string(const char *str, ScColor color) {
    sc_apply_colors(color, SC_ANSI_COLOR_NONE);
    fputs(str, sc_output_stream());
    fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());
}


/* ── render_inner_separator ─────────────────────────────────────────────── */

/**
 * Renders the horizontal separator between two data rows. Spanning columns
 * continue their content; non-spanning columns draw the inner border fill.
 */
void render_inner_separator(Table *table) {
    const ScTableData *data = table->table_data;
    ScBorderType style = table->opts.border.type;
    ScColor outer_color = table->opts.border.outer_color;
    bool no_outer = table->opts.border.no_outer;
    bool right_to_left = table->opts.right_to_left;

    const char *left_corner = border_char_sets[style].t_left;
    const char *right_corner = border_char_sets[style].t_right;
    if (table->has_rowspan && !no_outer) {
        adjust_corner_chars_for_rowspan(
            table, table->has_rowspan, &left_corner, &right_corner
        );
    }

    print_left_margin(table);
    if (!no_outer) { print_colored_string(left_corner, outer_color); }

    for (size_t i = 0; i < data->column_count; i++) {
        size_t col = right_to_left ? (data->column_count - 1 - i) : i;
        if (table->has_rowspan && table->has_rowspan[col]) {
            render_inner_sep_span_column(table, col);
        } else {
            render_inner_sep_fill(table, col);
        }
        if (i < data->column_count - 1) {
            render_inner_sep_junction(table, col);
        }
    }

    if (!no_outer) { print_colored_string(right_corner, outer_color); }
    fputc('\n', sc_output_stream());
}

/**
 * Renders one spanning column during an inner separator: continues the
 * cell's content when a span is active, otherwise prints blank space.
 */
static void render_inner_sep_span_column(Table *table, size_t col) {
    ScColor col_bg = table->table_data->columns[col].opts.bg;
    if (table->row_span && table->row_span[col].cell) {
        render_inner_sep_span_content(table, col, col_bg);
    } else {
        print_spaces_with_bg(table->column_widths[col], col_bg);
    }
}

/**
 * Builds the spanning cell's lines, selects the correct visual line for
 * the current separator row, and prints it between the left and right
 * cell padding.
 */
static void render_inner_sep_span_content(
    Table *table, size_t col, ScColor col_bg
) {
    const ScTableData *data = table->table_data;
    int content_width =
        table->column_widths[col] - table->pad_left - table->pad_right;
    if (content_width < 0) { content_width = 0; }

    size_t line_count;
    ScRenderLine *cell_lines =
        (data->columns[col].opts.word_wrap && content_width > 0)
            ? wrap_cell_lines(
                  table->row_span[col].cell, content_width, &line_count
              )
            : make_cell_lines(table->row_span[col].cell, &line_count);

    int line_index = rowspan_separator_line_index(
        &table->row_span[col], (int)line_count
    );

    print_spaces_with_bg(table->pad_left, col_bg);
    if (line_index >= 0 && line_index < (int)line_count) {
        print_tline_in_width(
            &cell_lines[line_index], content_width,
            data->columns[col].opts.halign, col_bg
        );
    } else {
        print_spaces_with_bg(content_width, col_bg);
    }
    print_spaces_with_bg(table->pad_right, col_bg);

    free_tlines(cell_lines, line_count);
}

/**
 * Returns the visual line index to draw for a spanning cell at the current
 * inner-separator row, accounting for vertical alignment within the span.
 */
static int rowspan_separator_line_index(
    const RowSpan *span, int content_lines
) {
    int extra = span->row_count - content_lines;
    if (extra < 0) { extra = 0; }

    int top_offset = 0;
    if (span->valign == SC_VALIGN_MIDDLE) { top_offset = extra / 2; }
    else if (span->valign == SC_VALIGN_BOTTOM) { top_offset = extra; }
    return span->line_offset - top_offset;
}

/** Prints the inner border fill for a normal (non-spanning) column. */
static void render_inner_sep_fill(Table *table, size_t col) {
    ScBorderType style = table->opts.border.type;
    sc_apply_colors(table->opts.border.inner_color, SC_ANSI_COLOR_NONE);
    for (int i = 0; i < table->column_widths[col]; i++) {
        fputs(border_char_sets[style].h, sc_output_stream());
    }
    fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());
}

/**
 * Prints the junction character between `col` and its neighbor, selecting
 * the glyph based on active rowspans on either side.
 */
static void render_inner_sep_junction(Table *table, size_t col) {
    if (!table_has_vertical_separator_after(table, col)) { return; }

    const bool *flags = table->has_rowspan;
    bool right_to_left = table->opts.right_to_left;
    ScBorderType style = table->opts.border.type;
    size_t neighbor_col = right_to_left ? (col - 1) : (col + 1);
    bool col_in_span = flags && flags[col];
    bool neighbor_in_span = flags && flags[neighbor_col];

    const char *junction_char = border_char_sets[style].cross;
    if (col_in_span && neighbor_in_span) {
        junction_char = border_char_sets[style].v;
    } else if (col_in_span) {
        junction_char = border_char_sets[style].t_left;
    } else if (neighbor_in_span) {
        junction_char = border_char_sets[style].t_right;
    }

    ScColor junction_color = table->opts.border.inner_color;
    if (table_is_header_column(table, col)
        && table->opts.border.header_col_sep_color.index != -2) {
        junction_color = table->opts.border.header_col_sep_color;
    }
    print_colored_string(junction_char, junction_color);
}


/* ── render_title_line ──────────────────────────────────────────────────── */

/**
 * Renders the top or bottom border line with the table title embedded.
 * Prints corner characters, then delegates the inner fill to
 * `render_title_inner`.
 */
void render_title_line(const Table *table, bool is_top) {
    ScBorderType style = table->opts.border.type;
    ScColor outer_color = table->opts.border.outer_color;
    const char *left_corner = is_top
        ? border_char_sets[style].tl : border_char_sets[style].bl;
    const char *right_corner = is_top
        ? border_char_sets[style].tr : border_char_sets[style].br;
    const char *fill_char = border_char_sets[style].h;
    int title_pad = table->opts.title.pad > 0
        ? table->opts.title.pad : DEFAULT_TITLE_PAD;

    print_left_margin(table);
    print_colored_string(left_corner, outer_color);
    render_title_inner(table, fill_char, outer_color, title_pad);
    print_colored_string(right_corner, outer_color);
    fputc('\n', sc_output_stream());
}

/** Dispatches between title-present and no-title inner rendering. */
static void render_title_inner(
    const Table *table, const char *fill_char,
    ScColor outer_color, int title_pad
) {
    if (table->opts.title.text && *table->opts.title.text) {
        render_title_with_fill(table, fill_char, outer_color, title_pad);
    } else {
        render_title_plain_fill(
            table->inner_width, fill_char, outer_color
        );
    }
}

/**
 * Computes left/right fill counts and prints: fill, padding, title text,
 * padding, fill — all in the outer-color / title-style combination.
 */
static void render_title_with_fill(
    const Table *table, const char *fill_char,
    ScColor outer_color, int title_pad
) {
    int title_length = (int)strlen(table->opts.title.text);
    int left_fill, right_fill;
    split_title_fill(
        table->inner_width, title_length, title_pad,
        table->opts.title.align, &left_fill, &right_fill
    );

    sc_apply_colors(outer_color, SC_ANSI_COLOR_NONE);
    for (int i = 0; i < left_fill; i++) { fputs(fill_char, sc_output_stream()); }
    fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());

    for (int i = 0; i < title_pad; i++) {
        print_colored_string(" ", outer_color);
    }
    sc_print(table->opts.title.text, table->opts.title.style);
    for (int i = 0; i < title_pad; i++) {
        print_colored_string(" ", outer_color);
    }

    sc_apply_colors(outer_color, SC_ANSI_COLOR_NONE);
    for (int i = 0; i < right_fill; i++) { fputs(fill_char, sc_output_stream()); }
    fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());
}

/**
 * Distributes the fill characters around the title text per `align`.
 * Both `*left_fill` and `*right_fill` are clamped to `>= 0`.
 */
static void split_title_fill(
    int inner_width, int title_length, int title_pad,
    ScHAlign align, int *left_fill, int *right_fill
) {
    int total_fill = inner_width - title_length - 2 * title_pad;
    if (total_fill < 0) { total_fill = 0; }

    *left_fill = 1;
    *right_fill = total_fill - 1;
    if (align == SC_ALIGN_CENTER) {
        *left_fill = total_fill / 2;
        *right_fill = total_fill - *left_fill;
    } else if (align == SC_ALIGN_RIGHT) {
        *left_fill = total_fill - 1;
        *right_fill = 1;
    }
    if (*left_fill < 0) { *left_fill = 0; }
    if (*right_fill < 0) { *right_fill = 0; }
}

/** Fills the entire `inner_width` of a title line with the border char. */
static void render_title_plain_fill(
    int inner_width, const char *fill_char, ScColor outer_color
) {
    sc_apply_colors(outer_color, SC_ANSI_COLOR_NONE);
    for (int i = 0; i < inner_width; i++) { fputs(fill_char, sc_output_stream()); }
    fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());
}


/* ── ScRenderLine printing helpers (shared with row.c) ─────────────────────────── */

/**
 * Prints `line` within `width` columns: adds alignment padding when the
 * content fits, or truncates span by span when the line is wider than
 * `width`.
 */
void print_tline_in_width(
    const ScRenderLine *line, int width, ScHAlign halign, ScColor bg
) {
    int remaining = width - (int)line->visible_width;
    if (remaining >= 0) {
        int left_pad = 0;
        int right_pad = remaining;
        if (halign == SC_ALIGN_CENTER) {
            left_pad = remaining / 2;
            right_pad = remaining - left_pad;
        } else if (halign == SC_ALIGN_RIGHT) {
            left_pad = remaining;
            right_pad = 0;
        }
        print_spaces_with_bg(left_pad, bg);
        for (size_t i = 0; i < line->count; i++) {
            print_span_with_bg(
                line->spans[i].text, line->spans[i].style, bg
            );
        }
        print_spaces_with_bg(right_pad, bg);
        return;
    }

    int remaining_cols = width;
    for (size_t i = 0; i < line->count && remaining_cols > 0; i++) {
        const char *text = line->spans[i].text;
        int span_width =
            (int)sc_utf8_string_length(text, strlen(text));
        if (span_width <= remaining_cols) {
            print_span_with_bg(text, line->spans[i].style, bg);
            remaining_cols -= span_width;
            continue;
        }
        size_t byte_count = sc_utf8_trim_to_cols(text, remaining_cols);
        char *clipped = strndup(text, byte_count);
        print_span_with_bg(clipped, line->spans[i].style, bg);
        free(clipped);
        remaining_cols = 0;
    }
}

/**
 * Prints a styled span, falling back to the cell background when the span
 * has no background color of its own.
 */
void print_span_with_bg(
    const char *text, ScTextStyle style, ScColor cell_bg
) {
    if (!sc_color_is_active(style.bg) && sc_color_is_active(cell_bg)) {
        style.bg = cell_bg;
    }
    sc_print(text, style);
}

/**
 * Prints `count` spaces with background color `bg` applied; reset after.
 */
void print_spaces_with_bg(int count, ScColor bg) {
    if (count <= 0) { return; }
    bool has_bg = sc_color_is_active(bg);
    if (has_bg) { sc_apply_colors(SC_ANSI_COLOR_NONE, bg); }
    for (int i = 0; i < count; i++) { fputc(' ', sc_output_stream()); }
    if (has_bg) { fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream()); }
}
