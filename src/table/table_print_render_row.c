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

/**
 * Per-row render context: derived once from table + row_section, passed through
 * the rendering stack to avoid recomputing shared state at every call site.
 */
typedef struct {
    const RowSpan *row_span;     /** Rowspan contexts; NULL for header/footer */
    ScColor        row_bg;       /** Resolved row background */
    RowSection     row_section;  /** Which table section this row belongs to */
    int            row_height;   /** Total visual height of this row */
    int            pad_left;     /** cell_pad.left */
    int            pad_right;    /** cell_pad.right */
    int            pad_vertical; /** cell_pad.top + cell_pad.bottom */
    int            pad_top;      /** cell_pad.top */
} RowRenderCtx;

/** Per-column TLine arrays built for one row, one entry per column. */
typedef struct {
    TLine  **lines;  /**< TLine array for each column;
                          NULL for skip/continuation columns. */
    size_t  *counts; /**< Number of TLines in each column's array. */
} ColumnLines;

/** Style and row context passed to the leaf cell-drawing functions. */
typedef struct {
    const ScTableOpts  *opts; /**< Table-level style options. */
    const RowRenderCtx *rctx; /**< Per-row rendering context. */
} CellDrawCtx;

/** Shared context for rendering one TLine of a cell. */
typedef struct {
    const TLine *line;    /**< The line to render. */
    ScColor      cell_bg; /**< Resolved cell background color. */
    CellDrawCtx  ctx;     /**< Style and row context. */
} CellLineCtx;

/** Shared context for building a single column's TLine array. */
typedef struct {
    const Table *table;         /**< The table being rendered. */
    size_t       column_index;  /**< Physical column index. */
    int pad_horiz; /**< Total horizontal cell padding(
        left + right
    ). */
    ColumnLines  column_lines; /**< Output buffer for the per-column TLine arrays. */
} ColBuildCtx;

/** Shared context for rendering the visual lines of a row. */
typedef struct {
    const Table        *table;        /**< The table being rendered. */
    const ScCell       *cells;        /**< The row's cell array. */
    ColumnLines         column_lines; /**< Per-column TLine arrays for this row. */
    const RowRenderCtx *rctx;         /**< Per-row rendering context. */
} RowDrawCtx;

// Forward declarations indented to reflect call hierarchy

static void render_row(
    const Table *table,
    const ScCell *cells,
    ScColor row_bg,
    int row_kind,
    int row_height_in
);
    static size_t build_row_cell_lines(
        const Table *table,
        const ScCell *cells,
        const RowRenderCtx *rctx,
        ColumnLines col_lines
    );
        static void build_rowspan_cont_col_lines(
            ColBuildCtx ctx, const RowRenderCtx *rctx
        );
        static size_t build_span_col_lines(ColBuildCtx ctx, const ScCell *cell);
            static int compute_span_w(
                const Table *table, size_t col, int span_cols
            );
    static void render_row_visual_line(RowDrawCtx ctx, int visual_line);
        static void render_row_cell(
            RowDrawCtx ctx,
            size_t column_index,
            size_t col_iter,
            int span_cols,
            int visual_line
        );
            static ScColor resolve_cell_bg(RowDrawCtx ctx, size_t column_index);
            static void resolve_cell_align(
                RowDrawCtx ctx,
                size_t column_index,
                ScHAlign *out_halign,
                ScVAlign *out_valign
            );
            static int compute_rowspan_cell_cli(
                const RowSpan *rowspan,
                int visual_line,
                int content_lines
            );
            static int compute_normal_cell_cli(
                int visual_line,
                int content_lines,
                ScVAlign valign,
                const RowRenderCtx *rctx
            );
            static void render_cell_line(
                CellLineCtx clx, int col_width, ScHAlign halign
            );
                static void print_cell_line_aligned(
                    CellLineCtx clx, int align_padding, ScHAlign halign
                );
                    static ScTextStyle apply_hdrftr_style(
                        ScTextStyle span_style, CellDrawCtx ctx
                    );
                static void print_cell_line_truncated(
                    CellLineCtx clx, int col_width
                );
            static void render_row_vsep(
                const Table *table,
                size_t column_index,
                size_t col_iter,
                int span_cols
            );

/**
 * Initialises a `RowRenderCtx`, builds per-column TLine arrays, dispatches each
 * visual line to `render_row_visual_line()`, then frees the arrays.
 */
static void render_row(
    const Table *table,
    const ScCell *cells,
    ScColor row_bg,
    int row_kind,
    int row_height_in
) {
    const ScTableData *table_data = table->table_data;

    ColumnLines cl = {
        .lines  = malloc(table_data->column_count * sizeof(TLine *)),
        .counts = malloc(table_data->column_count * sizeof(size_t)),
    };

    RowRenderCtx rctx = {
        .row_span    = (row_kind == 0) ? table->row_span : NULL,
        .row_bg      = row_bg,
        .row_section = (RowSection)row_kind,
        .pad_left     = table->opts.cell_pad.left,
        .pad_right    = table->opts.cell_pad.right,
        .pad_vertical = table->opts.cell_pad.top + table->opts.cell_pad.bottom,
        .pad_top      = table->opts.cell_pad.top,
    };

    size_t max_content_lines = build_row_cell_lines(table, cells, &rctx, cl);
    rctx.row_height = row_height_in > 0 ?
        row_height_in : (int)max_content_lines + rctx.pad_vertical;
    if (rctx.row_height < 1) {
        rctx.row_height = 1;
    }

    RowDrawCtx draw_ctx = {
        .table = table,
        .cells = cells,
        .column_lines = cl,
        .rctx = &rctx
    };
    for (int visual_line = 0; visual_line < rctx.row_height; visual_line++) {
        render_row_visual_line(draw_ctx, visual_line);
    }

    for (size_t col = 0; col < table_data->column_count; col++) {
        if (cl.lines[col]) { free_tlines(cl.lines[col], cl.counts[col]); }
    }
    free(cl.lines);
    free(cl.counts);
}

/**
 * Loops over every column, dispatching to `build_rowspan_cont_col_lines()`
 * or `build_span_col_lines()`.
 * Returns the tallest non-rowspan-starting line count.
 */
static size_t build_row_cell_lines(
    const Table *table,
    const ScCell *cells,
    const RowRenderCtx *rctx,
    ColumnLines column_lines
) {
    const ScTableData *table_data = table->table_data;
    int right_to_left = table->opts.right_to_left;
    int pad_horiz     = rctx->pad_left + rctx->pad_right;
    size_t max_content_lines = 0;

    for (size_t i = 0; i < table_data->column_count; i++) {
        size_t column_index =
            right_to_left ? (table_data->column_count - 1 - i) : i;

        if (cells[column_index].col_span < 0) {
            column_lines.lines[column_index] = NULL;
            column_lines.counts[column_index] = 0;
            continue;
        }

        ColBuildCtx cbctx = {
            .table = table,
            .column_index = column_index,
            .pad_horiz = pad_horiz,
            .column_lines = column_lines
        };

        if (cells[column_index].row_span == -1) {
            build_rowspan_cont_col_lines(cbctx, rctx);
            continue;
        }

        size_t line_count  = build_span_col_lines(cbctx, &cells[column_index]);
        int    row_span_val = cells[column_index].row_span;
        if (
            (row_span_val == 0 || row_span_val == 1) &&
            line_count > max_content_lines)
        {
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
static void build_rowspan_cont_col_lines(
    ColBuildCtx ctx, const RowRenderCtx *rctx
) {
    if (rctx->row_span && rctx->row_span[ctx.column_index].cell) {
        const ScTableData *table_data  = ctx.table->table_data;
        int span_content_width =
            ctx.table->column_widths[ctx.column_index] - ctx.pad_horiz;
        if (span_content_width < 0) {
            span_content_width = 0;
        }
        if (
            table_data->columns[ctx.column_index].opts.word_wrap &&
            span_content_width > 0
        ) {
            ctx.column_lines.lines[ctx.column_index] =
                wrap_cell_lines(
                    rctx->row_span[ctx.column_index].cell,
                    span_content_width,
                    &ctx.column_lines.counts[ctx.column_index]
                );
        } else {
            ctx.column_lines.lines[ctx.column_index] =
                make_cell_lines(
                    rctx->row_span[ctx.column_index].cell,
                    &ctx.column_lines.counts[ctx.column_index]
                );
        }
    } else {
        ctx.column_lines.lines[ctx.column_index] = NULL;
        ctx.column_lines.counts[ctx.column_index] = 0;
    }
}

/**
 * Builds the TLine array for a normal or colspan-initiating cell at
 * column `col`. Selects word-wrap or plain building based on column settings.
 * Returns the content line count written to `col_line_counts[col]`.
 */
static size_t build_span_col_lines(ColBuildCtx ctx, const ScCell *cell) {
    const ScTableData *table_data = ctx.table->table_data;
    int col_span  = cell->col_span;
    int span_cols = (col_span > 1) ? col_span : 1;
    if ((int)ctx.column_index + span_cols > (int)table_data->column_count) {
        span_cols = (int)table_data->column_count - (int)ctx.column_index;
    }

    int content_width =
        compute_span_w(ctx.table, ctx.column_index, span_cols) - ctx.pad_horiz;
    if (content_width < 0) {
        content_width = 0;
    }

    if (
        table_data->columns[ctx.column_index].opts.word_wrap &&
        content_width > 0
    ) {
        ctx.column_lines.lines[ctx.column_index] = wrap_cell_lines(
            cell, content_width, &ctx.column_lines.counts[ctx.column_index]
        );
    } else {
        ctx.column_lines.lines[ctx.column_index] = make_cell_lines(
            cell, &ctx.column_lines.counts[ctx.column_index]
        );
    }

    return ctx.column_lines.counts[ctx.column_index];
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
            bool is_header_col = (
                table->opts.header.col && span_col == header_col_phys
            );
            if (!no_inner_v || is_header_col) { span_total++; }
        }
    }
    return span_total;
}

/**
 * Renders one visual line `visual_line` of the row: prints the left border,
 * loops over columns via `render_row_cell()`, then closes with the
 * right border.
 */
static void render_row_visual_line(RowDrawCtx ctx, int visual_line) {
    const ScTableData *table_data = ctx.table->table_data;
    ScBorderType border_style = ctx.table->opts.border.style;
    ScColor outer_color       = ctx.table->opts.border.outer_color;
    int no_outer              = ctx.table->opts.border.no_outer;
    int right_to_left         = ctx.table->opts.right_to_left;

    print_left_margin(ctx.table);
    if (!no_outer) {
        print_colored_string(border_char_sets[border_style].v, outer_color);
    }

    size_t col_iter = 0;
    while (col_iter < table_data->column_count) {
        size_t column_index
            = right_to_left ?
                  (table_data->column_count - 1 - col_iter) : col_iter;
        int col_span = ctx.cells[column_index].col_span;
        if (col_span < 0) { col_iter++; continue; }

        int span_cols = (col_span > 1) ? col_span : 1;
        if ((int)column_index + span_cols > (int)table_data->column_count) {
            span_cols = (int)table_data->column_count - (int)column_index;
        }

        render_row_cell(ctx, column_index, col_iter, span_cols, visual_line);
        col_iter += (size_t)span_cols;
    }

    if (!no_outer) {
        print_colored_string(border_char_sets[border_style].v, outer_color);
    }
    fputc('\n', stdout);
}

/**
 * Renders one cell (padding + content + separator) at visual line `visual_line`.
 * Resolves background, alignment, and content line index, then delegates.
 */
static void render_row_cell(
    RowDrawCtx ctx,
    size_t col,
    size_t col_iter,
    int span_cols,
    int visual_line
) {
    int content_width = compute_span_w(ctx.table, col, span_cols)
                            - (ctx.rctx->pad_left + ctx.rctx->pad_right);
    if (content_width < 0) {
        content_width = 0;
    }

    ScColor  cell_bg = resolve_cell_bg(ctx, col);
    ScHAlign halign; ScVAlign valign;
    resolve_cell_align(ctx, col, &halign, &valign);

    int content_line_count = ctx.column_lines.lines[col] ?
                                 (int)ctx.column_lines.counts[col] : 0;
    int row_span_val       = ctx.cells[col].row_span;
    int content_line_idx;
    if (
        (row_span_val > 1 || row_span_val == -1) &&
        ctx.rctx->row_span &&
        ctx.rctx->row_span[col].cell
    ) {
        content_line_idx = compute_rowspan_cell_cli(
            &ctx.rctx->row_span[col], visual_line, content_line_count
        );
    } else {
        content_line_idx = compute_normal_cell_cli(
            visual_line, content_line_count, valign, ctx.rctx
        );
    }

    CellDrawCtx draw_ctx = {
        .opts = &ctx.table->opts,
        .rctx = ctx.rctx
    };
    print_spaces_bg(ctx.rctx->pad_left, cell_bg);
    if (
        content_line_idx >= 0 &&
        content_line_idx < content_line_count &&
        ctx.column_lines.lines[col]
    ) {
        CellLineCtx clx = {
            .line = &ctx.column_lines.lines[col][content_line_idx],
            .cell_bg = cell_bg,
            .ctx = draw_ctx
        };
        render_cell_line(clx, content_width, halign);
    } else {
        print_spaces_bg(content_width, cell_bg);
    }
    print_spaces_bg(ctx.rctx->pad_right, cell_bg);

    render_row_vsep(ctx.table, col, col_iter, span_cols);
}

/**
 * Resolves the effective cell background applying priority:
 * header/footer row/col bg > explicit `row_bg` > column bg.
 */
static ScColor resolve_cell_bg(RowDrawCtx ctx, size_t col) {
    const ScTableData *table_data  = ctx.table->table_data;
    int right_to_left= ctx.table->opts.right_to_left;
    bool is_header   = (ctx.rctx->row_section == ROW_SECTION_HEADER);
    bool is_footer   = (ctx.rctx->row_section == ROW_SECTION_FOOTER);
    size_t header_col_phys = right_to_left ? (table_data->column_count - 1) : 0;

    ScColor col_bg  = table_data->columns[col].opts.bg;
    ScColor cell_bg = (ctx.rctx->row_bg.index != -2) ? ctx.rctx->row_bg : col_bg;
    bool is_header_col = (ctx.table->opts.header.col && col == header_col_phys);

    if (is_header_col && !is_header) {
        cell_bg = is_footer ? ctx.table->opts.footer.col_bg :
                              ctx.table->opts.header.col_bg;
    }
    if (is_header)                   { cell_bg = ctx.table->opts.header.row_bg; }
    if (is_footer && !is_header_col) { cell_bg = ctx.table->opts.footer.row_bg; }
    if (is_footer &&  is_header_col) { cell_bg = ctx.table->opts.footer.col_bg; }

    return cell_bg;
}

/**
 * Resolves the horizontal and vertical alignment for column `col`,
 * applying per-cell overrides on top of the column defaults.
 */
static void resolve_cell_align(RowDrawCtx ctx, size_t col,
                                ScHAlign *out_halign, ScVAlign *out_valign) {
    const ScTableData *table_data = ctx.table->table_data;
    *out_halign = table_data->columns[col].opts.halign;
    *out_valign = table_data->columns[col].opts.valign;
    if (ctx.cells[col].align_set)  { *out_halign = ctx.cells[col].align; }
    if (ctx.cells[col].valign_set) { *out_valign = ctx.cells[col].valign; }
}

/**
 * Computes the content line index for a rowspan cell at `visual_line`,
 * distributing lines across the full span height with vertical alignment.
 */
static int compute_rowspan_cell_cli(
    const RowSpan *rowspan, int visual_line, int content_lines
) {
    int extra_lines = rowspan->row_count - content_lines;
    if (extra_lines < 0) { extra_lines = 0; }
    int top_offset = 0;
    if (rowspan->valign == SC_VALIGN_MIDDLE) {
        top_offset = extra_lines / 2;
    }
    else if (rowspan->valign == SC_VALIGN_BOTTOM) {
        top_offset = extra_lines;
    }
    return rowspan->line_offset + visual_line - top_offset;
}

/**
 * Computes the content line index for a normal (non-rowspan) cell at visual line
 * `visual_line`, applying vertical alignment within the row height.
 */
static int compute_normal_cell_cli(
    int visual_line,
    int content_lines,
    ScVAlign valign,
    const RowRenderCtx *rctx
) {
    int extra_lines = rctx->row_height - content_lines - rctx->pad_vertical;
    if (extra_lines < 0) { extra_lines = 0; }
    int top_pad = rctx->pad_top;
    if      (valign == SC_VALIGN_MIDDLE) { top_pad += extra_lines / 2; }
    else if (valign == SC_VALIGN_BOTTOM) { top_pad += extra_lines; }
    return visual_line - top_pad;
}

/**
 * Dispatches to `print_cell_line_aligned()` when content fits
 * within `col_width` or `print_cell_line_truncated()` when it is wider.
 */
static void render_cell_line(CellLineCtx clx, int col_width, ScHAlign halign) {
    int align_padding = col_width - (int)clx.line->visible_width;
    if (align_padding >= 0) {
        print_cell_line_aligned(clx, align_padding, halign);
    } else {
        print_cell_line_truncated(clx, col_width);
    }
}

/**
 * Prints all spans of `line` with left/right alignment padding
 * totalling `align_padding`.
 */
static void print_cell_line_aligned(
    CellLineCtx clx, int align_padding, ScHAlign halign
) {
    int left_pad  = 0;
    int right_pad = align_padding;
    if (halign == SC_ALIGN_CENTER) {
        left_pad = align_padding / 2;
        right_pad = align_padding - left_pad;
    }
    else if (halign == SC_ALIGN_RIGHT)  {
        left_pad = align_padding;
        right_pad = 0;
    }
    print_spaces_bg(left_pad, clx.cell_bg);
    for (size_t i = 0; i < clx.line->count; i++) {
        ScTextStyle span_style = apply_hdrftr_style(
            clx.line->spans[i].opts, clx.ctx
        );
        print_span_bg(clx.line->spans[i].text, span_style, clx.cell_bg);
    }
    print_spaces_bg(right_pad, clx.cell_bg);
}

/**
 * Returns `span_style` with header or footer text style applied to unstyled
 * spans: only sets `attr`/`fg` when the span carries none of its own.
 */
static ScTextStyle apply_hdrftr_style(ScTextStyle span_style, CellDrawCtx ctx) {
    if (ctx.rctx->row_section == ROW_SECTION_HEADER) {
        if (span_style.attr == 0) {
            span_style.attr = ctx.opts->header.opts.attr;
        }
        if (span_style.fg.index == -2) {
            span_style.fg   = ctx.opts->header.opts.fg;
        }
    } else if (ctx.rctx->row_section == ROW_SECTION_FOOTER) {
        if (span_style.attr == 0) {
            span_style.attr = ctx.opts->footer.opts.attr;
        }
        if (span_style.fg.index == -2) {
            span_style.fg   = ctx.opts->footer.opts.fg;
        }
    }
    return span_style;
}

/**
 * Prints as many columns of `line` as fit within `col_width`, truncating the
 * last span at the exact column boundary when needed.
 */
static void print_cell_line_truncated(CellLineCtx clx, int col_width) {
    int cols_remaining = col_width;
    for (size_t i = 0; i < clx.line->count && cols_remaining > 0; i++) {
        ScTextStyle span_style = apply_hdrftr_style(
            clx.line->spans[i].opts, clx.ctx
        );
        const char *span_text  = clx.line->spans[i].text;
        int span_width = (int)sc_utf8_string_length(
            span_text, strlen(span_text)
        );

        if (span_width <= cols_remaining) {
            print_span_bg(span_text, span_style, clx.cell_bg);
            cols_remaining -= span_width;
        } else {
            size_t trimmed_bytes = sc_utf8_trim_to_cols(
                span_text, cols_remaining
            );
            char  *trimmed_text  = strndup(span_text, trimmed_bytes);
            print_span_bg(trimmed_text, span_style, clx.cell_bg);
            free(trimmed_text); cols_remaining = 0;
        }
    }
}

/**
 * Prints the vertical separator after the cell at column `col`, selecting
 * the header-column separator color when applicable.
 */
static void render_row_vsep(
    const Table *table, size_t col, size_t col_iter, int span_cols
) {
    const ScTableData *table_data = table->table_data;
    ScBorderType border_style  = table->opts.border.style;
    ScColor inner_color        = table->opts.border.inner_color;
    int no_inner_v             = table->opts.border.no_inner_v;
    int right_to_left          = table->opts.right_to_left;
    size_t header_col_phys     = right_to_left ?
                                 (table_data->column_count - 1) : 0;

    size_t end_col = col + (size_t)span_cols - 1;
    if (
        col_iter + (size_t)span_cols < table_data->column_count &&
        border_style != SC_BORDER_NONE
    ) {
        bool is_header_col_end = (
            table->opts.header.col &&
            end_col == header_col_phys
        );
        if (!no_inner_v || is_header_col_end) {
            ScColor sep_color = inner_color;
            if (
                is_header_col_end &&
                table->opts.border.header_col_sep_color.index != -2
            ) {
                sep_color = table->opts.border.header_col_sep_color;
            }
            print_colored_string(border_char_sets[border_style].v, sep_color);
        }
    }
}
