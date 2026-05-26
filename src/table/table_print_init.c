#pragma once

#include "sparcli.h"             // IWYU pragma: export
#include "internal.h"            // IWYU pragma: export
#include "table_internal.h"      // IWYU pragma: export
#include "table_print_render.c"  // IWYU pragma: export
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


static void compute_column_widths(Table *table);
    static size_t get_max_content_width(const ScTableData *td, size_t c);
        static size_t get_header_width(const TCol *col);
        static size_t max_cell_width_in_row_group(
            const TRow *rows, size_t row_count, size_t column_index
        );
    static int apply_column_constraints(
        size_t content_width, ScEdges cell_pad, const ScColOpts *column_options
    );
static void apply_total_width(Table *table);
    static int compute_rendered_table_width(const Table *table);
        static int count_inner_separators(const Table *table);
    static int count_flex_columns(const ScTableData *td);
    static void distribute_width_delta(
        Table *table, int delta, int flex_column_count
    );
static int get_table_inner_width(const Table *table);
static int *compute_row_heights(const Table *table);
    static int compute_row_height(
        const Table *table, size_t row_index, int v_padding, int h_padding
        );
        static size_t tallest_cell_line_count(
            const Table *table, size_t row_index, int h_padding
        );
            static size_t cell_line_count(
                const ScCell *cell,
                const ScColOpts *col_opts,
                int available_width
            );


/**
 * Initializes a render struct: allocates per-column arrays and computes the
 * full layout.
 */
static void table_init(
    Table *table, const ScTableData *table_data, ScTableOpts opts
) {
    table->table_data  = table_data;
    table->opts        = opts;
    table->column_widths  = malloc(table_data->column_count * sizeof(int));
    table->is_rs       = calloc(table_data->column_count, sizeof(int));
    table->rsc         = calloc(table_data->column_count, sizeof(RSCtx));

    compute_column_widths(table);
    apply_total_width(table);
    table->inner_w     = get_table_inner_width(table);
    table->row_heights = compute_row_heights(table);
}

/** Computes and stores the display width of each column. */
static void compute_column_widths(Table *table) {
    const ScTableData *td = table->table_data;
    for (size_t i = 0; i < td->column_count; i++) {
        size_t max_content_width = get_max_content_width(td, i);
        table->column_widths[i] = apply_column_constraints(
            max_content_width, table->opts.cell_pad, &td->columns[i].opts
        );
    }
}

/**
 * Returns the widest content across header, data rows and footer rows for
 * the given column.
 */
static size_t get_max_content_width(const ScTableData *td, size_t c) {
    return max_sz_n((size_t[]){
        get_header_width(&td->columns[c]),
        max_cell_width_in_row_group(td->rows,        td->row_count,        c),
        max_cell_width_in_row_group(td->footer_rows, td->footer_row_count, c),
    }, 3);
}

/** Returns the visible character width of a column's header string. */
static size_t get_header_width(const TCol *col) {
    const char *header = col->header;
    return header ? sc_utf8_string_length(header, strlen(header)) : 0;
}

/**
 * Returns the widest non-spanning cell in the given column across the given
 * row array.
 */
static size_t max_cell_width_in_row_group(
    const TRow *rows, size_t row_count, size_t column_index
) {
    size_t max_width = 0;
    for (size_t r = 0; r < row_count; r++) {
        if (column_index >= rows[r].ncols) continue;
        const ScCell *cell = &rows[r].cells[column_index];
        if (cell->colspan != 0 && cell->colspan != 1) continue;
        if (cell->rowspan == -1) continue;
        size_t w = cell_vis_width(cell);
        if (w > max_width) max_width = w;
    }
    return max_width;
}

/**
 * Adds cell padding to `content_width`, then clamps to fixed/min/max column
 * constraints.
 */
static int apply_column_constraints(
    size_t content_width, ScEdges cell_pad, const ScColOpts *column_options
) {
    int col_w = (int)content_width + cell_pad.left + cell_pad.right;

    // `fixed_width` overrides padding and min/max
    if (column_options->fixed_width > 0) {
        return column_options->fixed_width;
    }

    // Enforce lower bound if specified and if content width + pad less than min
    if (column_options->min_width > 0 && col_w < column_options->min_width) {
        col_w = column_options->min_width;
    }

    // Enforce upper bound if specified and if content width is greater than max
    if (column_options->max_width > 0 && col_w > column_options->max_width) {
        col_w = column_options->max_width;
    }

    return col_w;
}

/** Scales flex column widths so the table reaches `opts.total_width`. */
static void apply_total_width(Table *table) {
    if (table->opts.total_width <= 0) return;

    int delta = table->opts.total_width - compute_rendered_table_width(table);
    if (delta == 0) return;

    int nflex = count_flex_columns(table->table_data);
    if (nflex == 0) return;

    distribute_width_delta(table, delta, nflex);
}

/**
 * Returns the current total rendered width of the given table, including:
 * outer frame + separators + column widths.
 */
static int compute_rendered_table_width(const Table *table) {
    const ScTableData *td = table->table_data;
    ScTableBorder border = table->opts.border;

    int outer_frame_width = 0;
    if (border.style != SC_BORDER_NONE && !border.no_outer) {
        outer_frame_width = 2;
    }

    int width = outer_frame_width + count_inner_separators(table);
    for (size_t i = 0; i < td->column_count; i++) {
        width += table->column_widths[i];
    }

    return width;
}

/** Returns the number of visible vertical separators between columns. */
static int count_inner_separators(const Table *table) {
    const ScTableData *td = table->table_data;
   ScTableBorder border = table->opts.border;

    int count = 0;
    for (size_t c = 0; c + 1 < td->column_count; c++) {
        int is_hcol = (table->opts.header.col && c == 0);
        if (border.style != SC_BORDER_NONE && (!border.no_inner_v || is_hcol)) {
            count++;
        }
    }

    return count;
}

/** Returns the number of flex (non-fixed-width) columns. */
static int count_flex_columns(const ScTableData *td) {
    int count = 0;
    for (size_t c = 0; c < td->column_count; c++)
        if (td->columns[c].opts.fixed_width == 0) count++;
    return count;
}

/**
 * Distributes `delta` evenly across flex columns; remainder spread one column
 * at a time.
 */
static void distribute_width_delta(
    Table *table, int delta, int flex_column_count
) {
    // Integer division:
    // Each flex column gets `base_adjustment`,
    // `remaining` columns get one extra step,
    // `direction` handles both grow (delta > 0) and shrink (delta < 0).
    int base_adjustment  = delta / flex_column_count;
    int remaining  = delta - base_adjustment * flex_column_count;
    int direction = (remaining >= 0) ? 1 : -1;
    remaining = (remaining < 0) ? -remaining : remaining;

    const ScTableData *td = table->table_data;
    for (size_t i = 0; i < td->column_count; i++) {
        if (td->columns[i].opts.fixed_width > 0) continue;
        int col_adjustment = base_adjustment + (remaining > 0 ? direction : 0);
        if (remaining > 0) remaining--;
        int new_width = table->column_widths[i] + col_adjustment;
        if (new_width < 2) new_width = 2;
        table->column_widths[i] = new_width;
    }
}

/** Returns the sum of all column widths plus inner vertical separators. */
static int get_table_inner_width(const Table *table) {
    const ScTableData *table_data = table->table_data;

    int width = 0;
    for (size_t i = 0; i < table_data->column_count; i++) {
        // Add this column's width
        width += table->column_widths[i];

        // Add the separator after this column, if one exists
        if (
            i < table_data->column_count - 1 &&
            table->opts.border.style != SC_BORDER_NONE
        ) {
            int is_hcol = (table->opts.header.col && i == 0);
            if (!table->opts.border.no_inner_v || is_hcol) width++;
        }
    }
    return width;
}

/** Allocates and returns an array of visual heights for all data rows. */
static int *compute_row_heights(const Table *table) {
    const ScTableData *td = table->table_data;

    int v_padding = table->opts.cell_pad.top  + table->opts.cell_pad.bottom;
    int h_padding = table->opts.cell_pad.left + table->opts.cell_pad.right;
    int *heights = malloc(td->row_count * sizeof(int));
    for (size_t r = 0; r < td->row_count; r++) {
        heights[r] = compute_row_height(table, r, v_padding, h_padding);
    }

    return heights;
}

/**
 * Returns the visual height of a row: tallest cell content + vertical padding,
 * minimum 1.
 */
static int compute_row_height(
    const Table *table, size_t row_index, int v_padding, int h_padding
) {
    int height = (int)tallest_cell_line_count(table, row_index, h_padding) +
                 v_padding;
    return height < 1 ? 1 : height;
}

/** Returns the line count of the tallest non-spanning cell in the given row. */
static size_t tallest_cell_line_count(
    const Table *table, size_t row_index, int h_padding
) {
    const ScTableData *td = table->table_data;

    size_t max_lines = 0;
    for (size_t i = 0; i < td->column_count; i++) {
        const ScCell *cell = &td->rows[row_index].cells[i];
        if (cell->colspan < 0) continue;
        if (cell->rowspan < 0 || cell->rowspan > 1) continue;
        int available_width = table->column_widths[i] - h_padding;
        if (available_width < 0) available_width = 0;
        size_t lines = cell_line_count(
            cell, &td->columns[i].opts, available_width
        );
        if (lines > max_lines) max_lines = lines;
    }
    return max_lines;
}

/**
 * Returns the number of rendered lines for the given `cell`, applying
 * word-wrap if enabled.
 */
static size_t cell_line_count(
    const ScCell *cell, const ScColOpts *col_opts, int available_width
) {
    size_t line_count;
    TLine *lines = (col_opts->word_wrap && available_width > 0)
        ? wrap_cell_lines(cell, available_width, &line_count)
        : make_cell_lines(cell, &line_count);
    free_tlines(lines, line_count);
    return line_count;
}
