#pragma once

#include "sparcli.h"
#include "internal.h"
#include "table_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "table_print_render_cell.c"  // IWYU pragma: export

/* Visual spec for one horizontal border line: corner chars, fill, junction,
   and colors. fill_color and mid_color are unified into a single `color` field
   since all callers pass the same value for both. */
typedef struct {
    const char *left_corner_char;        /* left  corner char */
    const char *right_corner_char;        /* right corner char */
    const char *fill_char;      /* fill (horizontal) char, repeated per column */
    const char *column_separator;       /* junction char between columns; NULL = no junction */
    ScColor     inner_color;     /* fill and junction color */
    ScColor     edge_color;/* left/right corner color */

    /**
     * Flag for applying `header_col_sep_color` at column junctions.
     * True for  inner separator lines (header/footer boundaries) where
     * header_col_sep_color applies at column junctions.
     * False for outer frame lines (top/bottom border) where it does not.
     */
    bool use_header_col_sep;
} HBorderSpec;

// Forward declarations indented to reflect call hierarchy
static void print_colored_string(const char *str, ScColor fg);
static void print_spaces_bg(int count, ScColor bg);
static void print_span_bg(const char *text, ScTextStyle opts, ScColor cell_bg);
static void print_left_margin(const Table *table);
static void print_empty_lines(int count);

static void render_horizontal_border(const Table *table, HBorderSpec spec, const bool *rowspan_flags);
    static void adjust_hborder_corners(const Table *table, const bool *rowspan_flags, const char **left_corner, const char **right_corner);
    static void render_hborder_col_fill(const Table *table, size_t col, const bool *rowspan_flags, HBorderSpec spec);
    static void render_hborder_junction(const Table *table, size_t col, const bool *rowspan_flags, HBorderSpec spec);

static void render_inner_sep(Table *table);
    static void render_isep_span_col(Table *table, size_t col);
        static void render_isep_span_content(Table *table, size_t col, ScColor col_bg);
            static int  compute_span_cli(const RowSpan *span_ctx, int content_lines);
            static void print_tline_in_width(const TLine *line, int width, ScHAlign halign, ScColor bg);
    static void render_isep_border_fill(Table *table, size_t col);
    static void render_isep_junction(Table *table, size_t col);

static void render_title_line(const Table *table, int is_top);
    static void render_title_inner(const Table *table, const char *fill_char, ScColor outer_color, int title_pad);
        static void render_title_with_fill(const Table *table, const char *fill_char, ScColor outer_color, int title_pad);
            static void compute_title_fill_split(int inner_w, int title_len, int title_pad, ScHAlign align, int *left_fill, int *right_fill);
        static void render_title_full_fill(int inner_w, const char *fill_char, ScColor outer_color);


/* ── Low-level print helpers ─────────────────────────────────────────────── */

/**
 * Prints a single string `str` in foreground color `fg`, then resets.
 */
static void print_colored_string(const char *str, ScColor fg) {
    sc_apply_colors(fg, SC_ANSI_COLOR_NONE);
    fputs(str, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

/* Prints `count` space characters with background color `bg` applied, then resets. */
static void print_spaces_bg(int count, ScColor bg) {
    if (count <= 0) { return; }
    if (bg.index != -2) { sc_apply_colors(SC_ANSI_COLOR_NONE, bg); }
    for (int i = 0; i < count; i++) { fputc(' ', stdout); }
    if (bg.index != -2) { fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout); }
}

/* Prints a styled span, falling back to the cell background when the span
   has no background of its own. */
static void print_span_bg(const char *text, ScTextStyle opts, ScColor cell_bg) {
    if (opts.bg.index == -2 && cell_bg.index != -2) { opts.bg = cell_bg; }
    sc_print(text, opts);
}

/* Prints the left margin spaces for the table. */
static void print_left_margin(const Table *table) {
    for (int i = 0; i < table->opts.margin.left; i++) { fputc(' ', stdout); }
}

/* Prints `count` empty lines to apply vertical margins. */
static void print_empty_lines(int count) {
    for (int i = 0; i < count; i++) {
        fputc('\n', stdout);
    }
}

/* ── Horizontal border rendering ─────────────────────────────────────────── */

/** Renders one horizontal border line (top, bottom, or inner separator).
 *  Adjusts edge corners for active rowspans, then prints margin, left edge,
 *  per-column fill and junction chars, right edge, and newline. */
static void render_horizontal_border(
    const Table *table,
    HBorderSpec spec,
    const bool *rowspan_flags   /* rowspan_flags[col]=1 → col has active rowspan */
) {
    const ScTableData *table_data = table->table_data;
    bool right_to_left = table->opts.right_to_left;
    bool no_outer      = table->opts.border.no_outer;
    const char *left_corner  = spec.left_corner_char;
    const char *right_corner = spec.right_corner_char;

    if (rowspan_flags && !no_outer) {
        adjust_hborder_corners(
            table, rowspan_flags, &left_corner, &right_corner
        );
    }

    print_left_margin(table);
    if (!no_outer) { print_colored_string(left_corner, spec.edge_color); }

    for (size_t i = 0; i < table_data->column_count; i++) {
        size_t col = right_to_left ? (table_data->column_count - 1 - i) : i;
        render_hborder_col_fill(table, col, rowspan_flags, spec);
        if (i < table_data->column_count - 1 && spec.column_separator) {
            render_hborder_junction(table, col, rowspan_flags, spec);
        }
    }

    if (!no_outer) { print_colored_string(right_corner, spec.edge_color); }
    fputc('\n', stdout);
}

/** Replaces @p *lc / @p *rc with the vertical-bar glyph when the first or
 *  last rendered column has an active rowspan. */
static void adjust_hborder_corners(
    const Table *table, const bool *rowspan_flags,
    const char **left_corner, const char **right_corner
) {
    const ScTableData *table_data = table->table_data;
    ScBorderType border_style     = table->opts.border.style;
    bool right_to_left            = table->opts.right_to_left;
    size_t first_col = right_to_left ? (table_data->column_count - 1) : 0;
    size_t last_col  = right_to_left ? 0 : (table_data->column_count - 1);
    if (rowspan_flags[first_col]) {
        *left_corner  = border_char_sets[border_style].v;
    }
    if (rowspan_flags[last_col])  {
        *right_corner = border_char_sets[border_style].v;
    }
}

/** Prints the fill segment for column @p c: spaces when a rowspan is active,
 *  otherwise the repeated fill character from @p spec in @p spec.color. */
static void render_hborder_col_fill(
    const Table *table, size_t col, const bool *rowspan_flags, HBorderSpec spec
) {
    if (rowspan_flags && rowspan_flags[col]) {
        for (int i = 0; i < table->column_widths[col]; i++) {
            fputc(' ', stdout);
        }
    } else {
        sc_apply_colors(spec.inner_color, SC_ANSI_COLOR_NONE);
        for (int i = 0; i < table->column_widths[col]; i++) {
            fputs(spec.fill_char, stdout);
        }
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    }
}

/** Prints the junction character between column @p c and its neighbour.
 *  Selects the glyph based on which side has an active rowspan, and overrides
 *  the color for the header-column separator when @p spec.use_header_col_sep is set. */
static void render_hborder_junction(
    const Table *table, size_t col, const bool *rowspan_flags, HBorderSpec spec
) {
    const ScTableData *table_data = table->table_data;
    ScBorderType border_style     = table->opts.border.style;
    bool right_to_left  = table->opts.right_to_left;
    size_t header_col_idx = right_to_left ? (table_data->column_count - 1) : 0;
    bool is_header_col = (table->opts.header.col && col == header_col_idx);
    bool has_vert_sep  = !table->opts.border.no_inner_v || is_header_col;

    if (!has_vert_sep) { return; }

    const char *junction_char = spec.column_separator;
    if (rowspan_flags) {
        size_t neighbor_col        = right_to_left ? (col - 1) : (col + 1);
        bool col_has_rowspan       = rowspan_flags[col];
        bool neighbor_has_rowspan  = rowspan_flags[neighbor_col];
        if(col_has_rowspan && neighbor_has_rowspan) {
            junction_char = border_char_sets[border_style].v;
        }
        else if (col_has_rowspan) {
            junction_char = border_char_sets[border_style].t_left;
        }
        else if (neighbor_has_rowspan) {
            junction_char = border_char_sets[border_style].t_right;
        }
    }

    ScColor junction_color = spec.inner_color;
    if (
        spec.use_header_col_sep &&
        is_header_col &&
        table->opts.border.header_col_sep_color.index != -2
    ) {
        junction_color = table->opts.border.header_col_sep_color;
    }
    print_colored_string(junction_char, junction_color);
}

/** Renders the horizontal separator line between two data rows. Spanning columns
 *  continue their cell content; all others draw the inner border fill character. */
static void render_inner_sep(Table *table) {
    const ScTableData *table_data = table->table_data;
    const bool *has_rowspan       = table->has_rowspan;
    ScBorderType border_style     = table->opts.border.style;
    ScColor outer_color           = table->opts.border.outer_color;
    bool no_outer                 = table->opts.border.no_outer;
    bool right_to_left            = table->opts.right_to_left;

    const char *left_corner  = border_char_sets[border_style].t_left;
    const char *right_corner = border_char_sets[border_style].t_right;
    if (has_rowspan && !no_outer) {
        adjust_hborder_corners(table, has_rowspan, &left_corner, &right_corner);
    }

    print_left_margin(table);
    if (!no_outer) { print_colored_string(left_corner, outer_color); }

    for (size_t i = 0; i < table_data->column_count; i++) {
        size_t col = right_to_left ? (table_data->column_count - 1 - i) : i;
        if (has_rowspan && has_rowspan[col]) {
            render_isep_span_col(table, col);
        } else {
            render_isep_border_fill(table, col);
        }
        if (i < table_data->column_count - 1) {
            render_isep_junction(table, col);
        }
    }

    if (!no_outer) { print_colored_string(right_corner, outer_color); }
    fputc('\n', stdout);
}

/** Renders one spanning column during an inner separator: either the next
 *  content line of the spanning cell, or a blank fill when there is no cell. */
static void render_isep_span_col(Table *table, size_t col) {
    ScColor col_bg = table->table_data->columns[col].opts.bg;
    if (table->row_span && table->row_span[col].cell) {
        render_isep_span_content(table, col, col_bg);
    } else {
        print_spaces_bg(table->column_widths[col], col_bg);
    }
}

/** Builds the spanning cell's lines, selects the correct visual line for the
 *  current separator row, and prints it between left and right cell padding. */
static void render_isep_span_content(Table *table, size_t col, ScColor col_bg) {
    const ScTableData *table_data = table->table_data;
    int pad_left  = table->opts.cell_pad.left;
    int pad_right = table->opts.cell_pad.right;
    int content_w = table->column_widths[col] - pad_left - pad_right;
    if (content_w < 0) { content_w = 0; }

    size_t line_count;
    TLine *cell_lines = (table_data->columns[col].opts.word_wrap && content_w > 0)
        ? wrap_cell_lines(table->row_span[col].cell, content_w, &line_count)
        : make_cell_lines(table->row_span[col].cell, &line_count);

    int line_idx = compute_span_cli(&table->row_span[col], (int)line_count);

    print_spaces_bg(pad_left, col_bg);
    if (line_idx >= 0 && line_idx < (int)line_count) {
        print_tline_in_width(
            &cell_lines[line_idx],
            content_w,
            table_data->columns[col].opts.halign,
            col_bg
        );
    } else {
        print_spaces_bg(content_w, col_bg);
    }
    print_spaces_bg(pad_right, col_bg);

    free_tlines(cell_lines, line_count);
}

/** Returns the visual line index to render for a spanning cell at the current
 *  separator row, based on vertical alignment and remaining blank space. */
static int compute_span_cli(const RowSpan *span_ctx, int content_lines) {
    int extra = span_ctx->row_count - content_lines;
    if (extra < 0) { extra = 0; }

    int top = 0;
    if      (span_ctx->valign == SC_VALIGN_MIDDLE) { top = extra / 2; }
    else if (span_ctx->valign == SC_VALIGN_BOTTOM) { top = extra; }
    return span_ctx->line_offset - top;
}

/** Prints @p line within @p width columns: adds alignment padding when the
 *  content fits, or truncates span by span when it is wider than @p width. */
static void print_tline_in_width(
    const TLine *line, int width, ScHAlign halign, ScColor bg
) {
    int remaining_width = width - (int)line->vis_w;
    if (remaining_width >= 0) {
        int left_pad = 0, right_pad = remaining_width;
        if(halign == SC_ALIGN_CENTER) {
            left_pad = remaining_width / 2;
            right_pad = remaining_width - left_pad;
        }
        else if(halign == SC_ALIGN_RIGHT) {
            left_pad = remaining_width;
            right_pad = 0;
        }
        print_spaces_bg(left_pad, bg);
        for (size_t i = 0; i < line->count; i++) {
            print_span_bg(line->spans[i].text, line->spans[i].opts, bg);
        }
        print_spaces_bg(right_pad, bg);
    } else {
        int remaining_cols = width;
        for (size_t i = 0; i < line->count && remaining_cols > 0; i++) {
            const char *span_text = line->spans[i].text;
            int span_width = (int)sc_utf8_string_length(
                span_text, strlen(span_text)
            );
            if (span_width <= remaining_cols) {
                print_span_bg(span_text, line->spans[i].opts, bg);
                remaining_cols -= span_width;
            } else {
                size_t byte_count   = sc_utf8_trim_to_cols(
                    span_text, remaining_cols
                );
                char  *clipped_text = strndup(span_text, byte_count);
                print_span_bg(clipped_text, line->spans[i].opts, bg);
                free(clipped_text); remaining_cols = 0;
            }
        }
    }
}

/** Prints the inner border fill for a normal (non-spanning) column. */
static void render_isep_border_fill(Table *table, size_t col) {
    ScBorderType border_style = table->opts.border.style;
    ScColor inner_color       = table->opts.border.inner_color;
    sc_apply_colors(inner_color, SC_ANSI_COLOR_NONE);
    for (int i = 0; i < table->column_widths[col]; i++) {
        fputs(border_char_sets[border_style].h, stdout);
    }
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

/** Prints the junction character between column @p c and its neighbour,
 *  selecting the glyph based on active rowspans on either side. */
static void render_isep_junction(Table *table, size_t col) {
    const ScTableData *table_data = table->table_data;
    const bool *has_rowspan = table->has_rowspan;
    ScBorderType border_style = table->opts.border.style;
    ScColor inner_color = table->opts.border.inner_color;
    bool right_to_left = table->opts.right_to_left;
    size_t header_col_idx = right_to_left ? (table_data->column_count - 1) : 0;
    bool is_header_col = (table->opts.header.col && col == header_col_idx);
    bool has_vert_sep = !table->opts.border.no_inner_v || is_header_col;

    if (!has_vert_sep) { return; }

    size_t neighbor_col           = right_to_left ? (col - 1) : (col + 1);
    bool col_has_rowspan          = has_rowspan && has_rowspan[col];
    bool neighbor_has_rowspan     = has_rowspan && has_rowspan[neighbor_col];
    const char *junction_char = border_char_sets[border_style].cross;
    if(col_has_rowspan && neighbor_has_rowspan) {
        junction_char = border_char_sets[border_style].v;
    }
    else if (col_has_rowspan) {
        junction_char = border_char_sets[border_style].t_left;
    }
    else if (neighbor_has_rowspan)  {
        junction_char = border_char_sets[border_style].t_right;
    }
    ScColor junction_color = inner_color;
    if (is_header_col && table->opts.border.header_col_sep_color.index != -2) {
        junction_color = table->opts.border.header_col_sep_color;
    }
    print_colored_string(junction_char, junction_color);
}

/** Renders the top or bottom border line with the table title embedded in it.
 *  Prints the left/right corner chars and delegates the inner fill to
 *  render_title_inner(). */
static void render_title_line(const Table *table, int is_top) {
    ScBorderType border_style = table->opts.border.style;
    ScColor outer_color = table->opts.border.outer_color;
    const char *left_corner = is_top ? border_char_sets[border_style].tl
                                     : border_char_sets[border_style].bl;
    const char *right_corner = is_top ? border_char_sets[border_style].tr
                                      : border_char_sets[border_style].br;
    const char *fill_char = border_char_sets[border_style].h;
    int title_pad = table->opts.title.pad > 0 ? table->opts.title.pad : 1;

    print_left_margin(table);
    print_colored_string(left_corner, outer_color);
    render_title_inner(table, fill_char, outer_color, title_pad);
    print_colored_string(right_corner, outer_color);
    fputc('\n', stdout);
}

/** Dispatches between title-present and no-title rendering for the inner fill
 *  area of a title border line. */
static void render_title_inner(
    const Table *table, const char *fill_char, ScColor outer_color, int title_pad
) {
    if (table->opts.title.text && *table->opts.title.text) {
        render_title_with_fill(table, fill_char, outer_color, title_pad);
    } else {
        render_title_full_fill(table->inner_width, fill_char, outer_color);
    }
}

/** Computes the left/right fill counts and prints: fill, padding, title text,
 *  padding, fill — all in the outer-color / title-style combination. */
static void render_title_with_fill(
    const Table *table, const char *fill_char, ScColor outer_color, int title_pad
) {
    int title_len = (int)strlen(table->opts.title.text);
    int left_fill, right_fill;
    compute_title_fill_split(
        table->inner_width,
        title_len,
        title_pad,
        table->opts.title.align,
        &left_fill,
        &right_fill
    );

    sc_apply_colors(outer_color, SC_ANSI_COLOR_NONE);
    for (int i = 0; i < left_fill; i++) { fputs(fill_char, stdout); }
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    for (int i = 0; i < title_pad; i++) { print_colored_string(" ", outer_color); }
    sc_print(table->opts.title.text, table->opts.title.style);
    for (int i = 0; i < title_pad; i++) { print_colored_string(" ", outer_color); }
    sc_apply_colors(outer_color, SC_ANSI_COLOR_NONE);
    for (int i = 0; i < right_fill; i++) { fputs(fill_char, stdout); }
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

/** Computes the number of fill characters to the left (@p ld) and right (@p rd)
 *  of the title text, distributing them according to @p align. */
static void compute_title_fill_split(
    int inner_w,
    int title_len,
    int title_pad,
    ScHAlign align,
    int *left_fill,
    int *right_fill
) {
    int dashes = inner_w - title_len - 2 * title_pad;
    if (dashes < 0) {
        dashes = 0;
    }

    *left_fill = 1; *right_fill = dashes - 1;
    if(align == SC_ALIGN_CENTER) {
        *left_fill = dashes / 2;
        *right_fill = dashes - *left_fill;
    }
    else if (align == SC_ALIGN_RIGHT)  {
        *left_fill = dashes - 1;
        *right_fill = 1;
    }
    if (*left_fill  < 0) { *left_fill  = 0; }
    if (*right_fill < 0) { *right_fill = 0; }
}

/** Fills the entire @p inner_w of the title line with the border fill character
 *  when no title text is set. */
static void render_title_full_fill(
    int inner_w, const char *fill_char, ScColor outer_color
) {
    sc_apply_colors(outer_color, SC_ANSI_COLOR_NONE);
    for (int i = 0; i < inner_w; i++) { fputs(fill_char, stdout); }
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}
