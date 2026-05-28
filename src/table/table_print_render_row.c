#pragma once

#include "sparcli.h"
#include "internal.h"
#include "table_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "table_print_render_cell.c"
#include "table_print_render_border.c"

typedef enum {
    ROW_SECTION_DATA   = 0,
    ROW_SECTION_HEADER = 1,
    ROW_SECTION_FOOTER = 2,
} RowSection;

/* Per-row render context: derived once from table + row_section, passed through
   the rendering stack to avoid recomputing shared state at every call site. */
typedef struct {
    const RowSpan *row_span;     /* rowspan contexts; NULL for header/footer */
    ScColor        row_bg;       /* resolved row background */
    RowSection     row_section;  /* which table section this row belongs to */
    int            row_height;   /* total visual height of this row */
    int            pad_left;     /* cell_pad.left */
    int            pad_right;    /* cell_pad.right */
    int            pad_vertical; /* cell_pad.top + cell_pad.bottom */
    int            pad_top;      /* cell_pad.top */
} RowRenderCtx;

// Forward declarations indented to reflect call hierarchy

static void render_row(const Table *table, const ScCell *cells, ScColor row_bg, int row_kind, int row_height_in);
    static size_t build_row_cell_lines(const Table *table, const ScCell *cells, const RowRenderCtx *rctx, TLine **col_lines, size_t *col_line_counts);
        static void build_rowspan_cont_col_lines(const Table *table, size_t col, int pad_horiz, const RowRenderCtx *rctx, TLine **col_lines, size_t *col_line_counts);
        static size_t build_span_col_lines(const Table *table, const ScCell *cell, size_t col, int pad_horiz, TLine **col_lines, size_t *col_line_counts);
            static int compute_span_w(const Table *table, size_t col, int span_cols);
    static void render_row_visual_line(const Table *table, const ScCell *cells, TLine **col_lines, size_t *col_line_counts, int visual_line, const RowRenderCtx *rctx);
        static void render_row_cell(const Table *table, const ScCell *cells, TLine **col_lines, size_t *col_line_counts, size_t col, size_t col_iter, int span_cols, int visual_line, const RowRenderCtx *rctx);
            static ScColor resolve_cell_bg(const Table *table, size_t col, ScColor row_bg, RowSection row_section);
            static void resolve_cell_align(const Table *table, const ScCell *cells, size_t col, ScHAlign *out_halign, ScVAlign *out_valign);
            static int compute_rowspan_cell_cli(const RowSpan *rowspan, int visual_line, int content_lines);
            static int compute_normal_cell_cli(int visual_line, int content_lines, ScVAlign valign, const RowRenderCtx *rctx);
            static void render_cell_line(const TLine *line, int col_width, ScHAlign halign, ScColor cell_bg, const ScTableOpts *opts, const RowRenderCtx *rctx);
                static void print_cell_line_aligned(const TLine *line, int align_padding, ScHAlign halign, ScColor cell_bg, const ScTableOpts *opts, const RowRenderCtx *rctx);
                    static ScTextStyle apply_hdrftr_style(ScTextStyle span_style, const ScTableOpts *opts, const RowRenderCtx *rctx);
                static void print_cell_line_truncated(const TLine *line, int col_width, ScColor cell_bg, const ScTableOpts *opts, const RowRenderCtx *rctx);
            static void render_row_vsep(const Table *table, size_t col, size_t col_iter, int span_cols);


/* ── Row rendering ───────────────────────────────────────────────────────── */

/**
 * Initialises a `RowRenderCtx`, builds per-column TLine arrays, dispatches each
 * visual line to `render_row_visual_line()`, then frees the arrays.
 */
static void render_row(const Table *table, const ScCell *cells,
                        ScColor row_bg, int row_kind, int row_height_in) {
    const ScTableData *table_data = table->table_data;

    TLine **col_lines       = malloc(table_data->column_count * sizeof(TLine *));
    size_t *col_line_counts = malloc(table_data->column_count * sizeof(size_t));

    RowRenderCtx rctx = {
        .row_span    = (row_kind == 0) ? table->row_span : NULL,
        .row_bg      = row_bg,
        .row_section = (RowSection)row_kind,
        .pad_left     = table->opts.cell_pad.left,
        .pad_right    = table->opts.cell_pad.right,
        .pad_vertical = table->opts.cell_pad.top + table->opts.cell_pad.bottom,
        .pad_top      = table->opts.cell_pad.top,
    };

    size_t max_content_lines = build_row_cell_lines(table, cells, &rctx, col_lines, col_line_counts);
    rctx.row_height = row_height_in > 0 ? row_height_in : (int)max_content_lines + rctx.pad_vertical;
    if (rctx.row_height < 1) { rctx.row_height = 1; }

    for (int visual_line = 0; visual_line < rctx.row_height; visual_line++) {
        render_row_visual_line(table, cells, col_lines, col_line_counts, visual_line, &rctx);
    }

    for (size_t col = 0; col < table_data->column_count; col++) {
        if (col_lines[col]) { free_tlines(col_lines[col], col_line_counts[col]); }
    }
    free(col_lines);
    free(col_line_counts);
}

/**
 * Loops over every column, dispatching to `build_rowspan_cont_col_lines()` or
 * `build_span_col_lines()`. Returns the tallest non-rowspan-starting line count.
 */
static size_t build_row_cell_lines(const Table *table, const ScCell *cells,
                                    const RowRenderCtx *rctx,
                                    TLine **col_lines, size_t *col_line_counts) {
    const ScTableData *table_data = table->table_data;
    int right_to_left = table->opts.right_to_left;
    int pad_horiz     = rctx->pad_left + rctx->pad_right;
    size_t max_content_lines = 0;

    for (size_t col_iter = 0; col_iter < table_data->column_count; col_iter++) {
        size_t col = right_to_left ? (table_data->column_count - 1 - col_iter) : col_iter;

        if (cells[col].col_span < 0) {
            col_lines[col] = NULL; col_line_counts[col] = 0; continue;
        }

        if (cells[col].row_span == -1) {
            build_rowspan_cont_col_lines(table, col, pad_horiz, rctx, col_lines, col_line_counts);
            continue;
        }

        size_t line_count  = build_span_col_lines(table, &cells[col], col, pad_horiz, col_lines, col_line_counts);
        int    row_span_val = cells[col].row_span;
        if ((row_span_val == 0 || row_span_val == 1) && line_count > max_content_lines) {
            max_content_lines = line_count;
        }
    }

    return max_content_lines;
}

/**
 * Builds the TLine array for column `col` that continues a rowspan from a
 * previous row. Uses the originating cell from `rctx->row_span`; sets
 * `col_lines[col]=NULL` when no active span context exists.
 */
static void build_rowspan_cont_col_lines(const Table *table, size_t col, int pad_horiz,
                                          const RowRenderCtx *rctx,
                                          TLine **col_lines, size_t *col_line_counts) {
    if (rctx->row_span && rctx->row_span[col].cell) {
        const ScTableData *table_data  = table->table_data;
        int span_content_width = table->column_widths[col] - pad_horiz;
        if (span_content_width < 0) { span_content_width = 0; }
        if (table_data->columns[col].opts.word_wrap && span_content_width > 0) {
            col_lines[col] = wrap_cell_lines(rctx->row_span[col].cell, span_content_width, &col_line_counts[col]);
        } else {
            col_lines[col] = make_cell_lines(rctx->row_span[col].cell, &col_line_counts[col]);
        }
    } else {
        col_lines[col] = NULL; col_line_counts[col] = 0;
    }
}

/**
 * Builds the TLine array for a normal or colspan-initiating cell at column `col`.
 * Selects word-wrap or plain building based on column settings.
 * Returns the content line count written to `col_line_counts[col]`.
 */
static size_t build_span_col_lines(const Table *table, const ScCell *cell,
                                    size_t col, int pad_horiz,
                                    TLine **col_lines, size_t *col_line_counts) {
    const ScTableData *table_data = table->table_data;
    int col_span  = cell->col_span;
    int span_cols = (col_span > 1) ? col_span : 1;
    if ((int)col + span_cols > (int)table_data->column_count) {
        span_cols = (int)table_data->column_count - (int)col;
    }

    int content_width = compute_span_w(table, col, span_cols) - pad_horiz;
    if (content_width < 0) { content_width = 0; }

    if (table_data->columns[col].opts.word_wrap && content_width > 0) {
        col_lines[col] = wrap_cell_lines(cell, content_width, &col_line_counts[col]);
    } else {
        col_lines[col] = make_cell_lines(cell, &col_line_counts[col]);
    }

    return col_line_counts[col];
}

/**
 * Computes the raw column width for a colspan span, absorbing inner vertical
 * separator characters into the total.
 */
static int compute_span_w(const Table *table, size_t col, int span_cols) {
    const ScTableData *table_data  = table->table_data;
    int right_to_left              = table->opts.right_to_left;
    int no_inner_v                 = table->opts.border.no_inner_v;
    size_t header_col_phys = right_to_left ? (table_data->column_count - 1) : 0;

    int span_total = 0;
    for (int i = 0; i < span_cols; i++) {
        size_t span_col = col + (size_t)i;
        span_total += table->column_widths[span_col];
        if (i < span_cols - 1) {
            bool is_header_col = (table->opts.header.col && span_col == header_col_phys);
            if (!no_inner_v || is_header_col) { span_total++; }
        }
    }
    return span_total;
}

/**
 * Renders one visual line `visual_line` of the row: prints the left border, loops
 * over columns via `render_row_cell()`, then closes with the right border.
 */
static void render_row_visual_line(const Table *table, const ScCell *cells,
                                    TLine **col_lines, size_t *col_line_counts,
                                    int visual_line, const RowRenderCtx *rctx) {
    const ScTableData *table_data = table->table_data;
    ScBorderType border_style = table->opts.border.style;
    ScColor outer_color       = table->opts.border.outer_color;
    int no_outer              = table->opts.border.no_outer;
    int right_to_left         = table->opts.right_to_left;

    print_left_margin(table);
    if (!no_outer) { print_colored_string(border_char_sets[border_style].v, outer_color); }

    size_t col_iter = 0;
    while (col_iter < table_data->column_count) {
        size_t col   = right_to_left ? (table_data->column_count - 1 - col_iter) : col_iter;
        int col_span = cells[col].col_span;
        if (col_span < 0) { col_iter++; continue; }

        int span_cols = (col_span > 1) ? col_span : 1;
        if ((int)col + span_cols > (int)table_data->column_count) {
            span_cols = (int)table_data->column_count - (int)col;
        }

        render_row_cell(table, cells, col_lines, col_line_counts, col, col_iter, span_cols, visual_line, rctx);
        col_iter += (size_t)span_cols;
    }

    if (!no_outer) { print_colored_string(border_char_sets[border_style].v, outer_color); }
    fputc('\n', stdout);
}

/**
 * Renders one cell (padding + content + separator) at visual line `visual_line`.
 * Resolves background, alignment, and content line index, then delegates.
 */
static void render_row_cell(const Table *table, const ScCell *cells,
                             TLine **col_lines, size_t *col_line_counts,
                             size_t col, size_t col_iter, int span_cols, int visual_line,
                             const RowRenderCtx *rctx) {
    int content_width = compute_span_w(table, col, span_cols) - (rctx->pad_left + rctx->pad_right);
    if (content_width < 0) { content_width = 0; }

    ScColor  cell_bg = resolve_cell_bg(table, col, rctx->row_bg, rctx->row_section);
    ScHAlign halign; ScVAlign valign;
    resolve_cell_align(table, cells, col, &halign, &valign);

    int content_line_count = col_lines[col] ? (int)col_line_counts[col] : 0;
    int row_span_val       = cells[col].row_span;
    int content_line_idx;
    if ((row_span_val > 1 || row_span_val == -1) && rctx->row_span && rctx->row_span[col].cell) {
        content_line_idx = compute_rowspan_cell_cli(&rctx->row_span[col], visual_line, content_line_count);
    } else {
        content_line_idx = compute_normal_cell_cli(visual_line, content_line_count, valign, rctx);
    }

    print_spaces_bg(rctx->pad_left, cell_bg);
    if (content_line_idx >= 0 && content_line_idx < content_line_count && col_lines[col]) {
        render_cell_line(&col_lines[col][content_line_idx], content_width, halign, cell_bg, &table->opts, rctx);
    } else {
        print_spaces_bg(content_width, cell_bg);
    }
    print_spaces_bg(rctx->pad_right, cell_bg);

    render_row_vsep(table, col, col_iter, span_cols);
}

/**
 * Resolves the effective cell background applying priority:
 * header/footer row/col bg > explicit `row_bg` > column bg.
 */
static ScColor resolve_cell_bg(const Table *table, size_t col,
                                ScColor row_bg, RowSection row_section) {
    const ScTableData *table_data  = table->table_data;
    int right_to_left              = table->opts.right_to_left;
    bool is_header                 = (row_section == ROW_SECTION_HEADER);
    bool is_footer                 = (row_section == ROW_SECTION_FOOTER);
    size_t header_col_phys = right_to_left ? (table_data->column_count - 1) : 0;

    ScColor col_bg  = table_data->columns[col].opts.bg;
    ScColor cell_bg = (row_bg.index != -2) ? row_bg : col_bg;
    bool is_header_col = (table->opts.header.col && col == header_col_phys);

    if (is_header_col && !is_header) {
        cell_bg = is_footer ? table->opts.footer.col_bg : table->opts.header.col_bg;
    }
    if (is_header)                   { cell_bg = table->opts.header.row_bg; }
    if (is_footer && !is_header_col) { cell_bg = table->opts.footer.row_bg; }
    if (is_footer &&  is_header_col) { cell_bg = table->opts.footer.col_bg; }

    return cell_bg;
}

/**
 * Resolves the horizontal and vertical alignment for column `col`,
 * applying per-cell overrides on top of the column defaults.
 */
static void resolve_cell_align(const Table *table, const ScCell *cells, size_t col,
                                ScHAlign *out_halign, ScVAlign *out_valign) {
    const ScTableData *table_data = table->table_data;
    *out_halign = table_data->columns[col].opts.halign;
    *out_valign = table_data->columns[col].opts.valign;
    if (cells[col].align_set)  { *out_halign = cells[col].align; }
    if (cells[col].valign_set) { *out_valign = cells[col].valign; }
}

/**
 * Computes the content line index for a rowspan cell at visual line `visual_line`,
 * distributing lines across the full span height with vertical alignment.
 */
static int compute_rowspan_cell_cli(const RowSpan *rowspan, int visual_line, int content_lines) {
    int extra_lines = rowspan->row_count - content_lines;
    if (extra_lines < 0) { extra_lines = 0; }
    int top_offset = 0;
    if      (rowspan->valign == SC_VALIGN_MIDDLE) { top_offset = extra_lines / 2; }
    else if (rowspan->valign == SC_VALIGN_BOTTOM) { top_offset = extra_lines; }
    return rowspan->line_offset + visual_line - top_offset;
}

/**
 * Computes the content line index for a normal (non-rowspan) cell at visual line
 * `visual_line`, applying vertical alignment within the row height.
 */
static int compute_normal_cell_cli(int visual_line, int content_lines, ScVAlign valign,
                                    const RowRenderCtx *rctx) {
    int extra_lines = rctx->row_height - content_lines - rctx->pad_vertical;
    if (extra_lines < 0) { extra_lines = 0; }
    int top_pad = rctx->pad_top;
    if      (valign == SC_VALIGN_MIDDLE) { top_pad += extra_lines / 2; }
    else if (valign == SC_VALIGN_BOTTOM) { top_pad += extra_lines; }
    return visual_line - top_pad;
}

/**
 * Dispatches to `print_cell_line_aligned()` when content fits within `col_width`,
 * or `print_cell_line_truncated()` when it is wider.
 */
static void render_cell_line(const TLine *line, int col_width, ScHAlign halign,
                              ScColor cell_bg, const ScTableOpts *opts,
                              const RowRenderCtx *rctx) {
    int align_padding = col_width - (int)line->visible_width;
    if (align_padding >= 0) {
        print_cell_line_aligned(line, align_padding, halign, cell_bg, opts, rctx);
    } else {
        print_cell_line_truncated(line, col_width, cell_bg, opts, rctx);
    }
}

/**
 * Prints all spans of `line` with left/right alignment padding totalling `align_padding`.
 */
static void print_cell_line_aligned(const TLine *line, int align_padding, ScHAlign halign,
                                     ScColor cell_bg, const ScTableOpts *opts,
                                     const RowRenderCtx *rctx) {
    int left_pad  = 0;
    int right_pad = align_padding;
    if      (halign == SC_ALIGN_CENTER) { left_pad = align_padding / 2; right_pad = align_padding - left_pad; }
    else if (halign == SC_ALIGN_RIGHT)  { left_pad = align_padding;     right_pad = 0;                        }
    print_spaces_bg(left_pad, cell_bg);
    for (size_t i = 0; i < line->count; i++) {
        ScTextStyle span_style = apply_hdrftr_style(line->spans[i].opts, opts, rctx);
        print_span_bg(line->spans[i].text, span_style, cell_bg);
    }
    print_spaces_bg(right_pad, cell_bg);
}

/**
 * Returns `span_style` with header or footer text style applied to unstyled spans:
 * only sets `attr`/`fg` when the span carries none of its own.
 */
static ScTextStyle apply_hdrftr_style(ScTextStyle span_style, const ScTableOpts *opts,
                                       const RowRenderCtx *rctx) {
    if (rctx->row_section == ROW_SECTION_HEADER) {
        if (span_style.attr == 0)      { span_style.attr = opts->header.opts.attr; }
        if (span_style.fg.index == -2) { span_style.fg   = opts->header.opts.fg; }
    } else if (rctx->row_section == ROW_SECTION_FOOTER) {
        if (span_style.attr == 0)      { span_style.attr = opts->footer.opts.attr; }
        if (span_style.fg.index == -2) { span_style.fg   = opts->footer.opts.fg; }
    }
    return span_style;
}

/**
 * Prints as many columns of `line` as fit within `col_width`, truncating the last
 * span at the exact column boundary when needed.
 */
static void print_cell_line_truncated(const TLine *line, int col_width, ScColor cell_bg,
                                       const ScTableOpts *opts, const RowRenderCtx *rctx) {
    int cols_remaining = col_width;
    for (size_t i = 0; i < line->count && cols_remaining > 0; i++) {
        ScTextStyle span_style = apply_hdrftr_style(line->spans[i].opts, opts, rctx);
        const char *span_text  = line->spans[i].text;
        int span_width = (int)sc_utf8_string_length(span_text, strlen(span_text));
        if (span_width <= cols_remaining) {
            print_span_bg(span_text, span_style, cell_bg);
            cols_remaining -= span_width;
        } else {
            size_t trimmed_bytes = sc_utf8_trim_to_cols(span_text, cols_remaining);
            char  *trimmed_text  = strndup(span_text, trimmed_bytes);
            print_span_bg(trimmed_text, span_style, cell_bg);
            free(trimmed_text); cols_remaining = 0;
        }
    }
}

/**
 * Prints the vertical separator after the cell at column `col`, selecting
 * the header-column separator color when applicable.
 */
static void render_row_vsep(const Table *table, size_t col, size_t col_iter, int span_cols) {
    const ScTableData *table_data = table->table_data;
    ScBorderType border_style  = table->opts.border.style;
    ScColor inner_color        = table->opts.border.inner_color;
    int no_inner_v             = table->opts.border.no_inner_v;
    int right_to_left          = table->opts.right_to_left;
    size_t header_col_phys     = right_to_left ? (table_data->column_count - 1) : 0;

    size_t end_col = col + (size_t)span_cols - 1;
    if (col_iter + (size_t)span_cols < table_data->column_count && border_style != SC_BORDER_NONE) {
        bool is_header_col_end = (table->opts.header.col && end_col == header_col_phys);
        if (!no_inner_v || is_header_col_end) {
            ScColor sep_color = inner_color;
            if (is_header_col_end && table->opts.border.header_col_sep_color.index != -2) {
                sep_color = table->opts.border.header_col_sep_color;
            }
            print_colored_string(border_char_sets[border_style].v, sep_color);
        }
    }
}
