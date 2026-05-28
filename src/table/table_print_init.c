#include "sparcli.h"
#include "internal.h"
#include "table/table_internal.h"

#include <stdlib.h>
#include <string.h>


/** Minimum allowed column width after distributing the total-width delta. */
#define MIN_COLUMN_WIDTH 2


// Forward declarations indented to reflect call hierarchy
static void cache_padding_values(Table *table);

static void compute_column_widths(Table *table);
    static size_t max_content_width_for_column(
        const ScTableData *data, size_t col_index
    );
        static size_t header_width(const TCol *column);
        static size_t max_cell_width_in_rows(
            const TRow *rows, size_t row_count, size_t col_index
        );
    static int apply_column_constraints(
        size_t content_width, ScEdges cell_pad, const ScColOpts *col_opts
    );

static void apply_total_width(Table *table);
    static int current_rendered_width(const Table *table);
        static int count_inner_separators(const Table *table);
    static int count_flex_columns(const ScTableData *data);
    static void distribute_width_delta(
        Table *table, int delta, int flex_column_count
    );

static int compute_inner_width(const Table *table);

static int *compute_row_heights(const Table *table);
    static int compute_row_height(const Table *table, size_t row_index);
        static size_t tallest_cell_line_count(
            const Table *table, size_t row_index
        );


void table_init(
    Table *table, const ScTableData *table_data, ScTableOpts opts
) {
    table->table_data = table_data;
    table->opts = opts;
    table->column_widths = malloc(table_data->column_count * sizeof(int));
    table->has_rowspan = calloc(table_data->column_count, sizeof(bool));
    table->row_span = calloc(table_data->column_count, sizeof(RowSpan));

    cache_padding_values(table);
    compute_column_widths(table);
    apply_total_width(table);
    table->inner_width = compute_inner_width(table);
    table->row_heights = compute_row_heights(table);
}


/** Stores the four cell-padding values that don't change during rendering. */
static void cache_padding_values(Table *table) {
    table->pad_left = table->opts.cell_pad.left;
    table->pad_right = table->opts.cell_pad.right;
    table->pad_top = table->opts.cell_pad.top;
    table->pad_vertical =
        table->opts.cell_pad.top + table->opts.cell_pad.bottom;
}


/* ── Column widths ─────────────────────────────────────────────────────── */

/** Computes and stores the display width of each column. */
static void compute_column_widths(Table *table) {
    const ScTableData *data = table->table_data;
    for (size_t col = 0; col < data->column_count; col++) {
        size_t content_width = max_content_width_for_column(data, col);
        table->column_widths[col] = apply_column_constraints(
            content_width, table->opts.cell_pad, &data->columns[col].opts
        );
    }
}

/**
 * Returns the widest content across header, data rows and footer rows for
 * the given column.
 */
static size_t max_content_width_for_column(
    const ScTableData *data, size_t col_index
) {
    return max_sz_n((size_t[]){
        header_width(&data->columns[col_index]),
        max_cell_width_in_rows(data->rows, data->row_count, col_index),
        max_cell_width_in_rows(
            data->footer_rows, data->footer_row_count, col_index
        ),
    }, 3);
}

/** Returns the visible width of the column's header string. */
static size_t header_width(const TCol *column) {
    return column->header
        ? sc_utf8_string_length(column->header, strlen(column->header))
        : 0;
}

/**
 * Returns the widest non-spanning cell in `col_index` across the given
 * row array.
 */
static size_t max_cell_width_in_rows(
    const TRow *rows, size_t row_count, size_t col_index
) {
    size_t max_width = 0;
    for (size_t row = 0; row < row_count; row++) {
        if (col_index >= rows[row].ncols) { continue; }
        const ScCell *cell = &rows[row].cells[col_index];
        if (cell->col_span != 0 && cell->col_span != 1) { continue; }
        if (cell->row_span == -1) { continue; }
        size_t width = cell_visible_width(cell);
        if (width > max_width) { max_width = width; }
    }
    return max_width;
}

/**
 * Adds cell padding to `content_width`, then clamps to the fixed/min/max
 * constraints defined on the column.
 */
static int apply_column_constraints(
    size_t content_width, ScEdges cell_pad, const ScColOpts *col_opts
) {
    int width = (int)content_width + cell_pad.left + cell_pad.right;

    if (col_opts->fixed_width > 0) { return col_opts->fixed_width; }

    if (col_opts->min_width > 0 && width < col_opts->min_width) {
        width = col_opts->min_width;
    }
    if (col_opts->max_width > 0 && width > col_opts->max_width) {
        width = col_opts->max_width;
    }
    return width;
}


/* ── Total width distribution ──────────────────────────────────────────── */

/** Scales flex column widths so the table reaches `opts.total_width`. */
static void apply_total_width(Table *table) {
    if (table->opts.total_width <= 0) { return; }

    int delta = table->opts.total_width - current_rendered_width(table);
    if (delta == 0) { return; }

    int flex_count = count_flex_columns(table->table_data);
    if (flex_count == 0) { return; }

    distribute_width_delta(table, delta, flex_count);
}

/**
 * Returns the table's current rendered width: outer frame + inner
 * separators + sum of column widths.
 */
static int current_rendered_width(const Table *table) {
    const ScTableData *data = table->table_data;
    ScTableBorder border = table->opts.border;

    int outer_frame_width = 0;
    if (border.type != SC_BORDER_NONE && !border.no_outer) {
        outer_frame_width = 2;
    }

    int width = outer_frame_width + count_inner_separators(table);
    for (size_t col = 0; col < data->column_count; col++) {
        width += table->column_widths[col];
    }
    return width;
}

/** Counts the number of visible vertical separators between columns. */
static int count_inner_separators(const Table *table) {
    int count = 0;
    for (size_t col = 0; col + 1 < table->table_data->column_count; col++) {
        if (table_has_vertical_separator_after(table, col)) { count++; }
    }
    return count;
}

/** Returns the number of flex (non-fixed-width) columns. */
static int count_flex_columns(const ScTableData *data) {
    int count = 0;
    for (size_t col = 0; col < data->column_count; col++) {
        if (data->columns[col].opts.fixed_width == 0) { count++; }
    }
    return count;
}

/**
 * Distributes `delta` evenly across flex columns; the remainder is spread
 * one column at a time.
 */
static void distribute_width_delta(
    Table *table, int delta, int flex_column_count
) {
    int base_adjustment = delta / flex_column_count;
    int remainder = delta - base_adjustment * flex_column_count;
    int direction = (remainder >= 0) ? 1 : -1;
    if (remainder < 0) { remainder = -remainder; }

    const ScTableData *data = table->table_data;
    for (size_t col = 0; col < data->column_count; col++) {
        if (data->columns[col].opts.fixed_width > 0) { continue; }
        int adjustment = base_adjustment + (remainder > 0 ? direction : 0);
        if (remainder > 0) { remainder--; }
        int new_width = table->column_widths[col] + adjustment;
        if (new_width < MIN_COLUMN_WIDTH) { new_width = MIN_COLUMN_WIDTH; }
        table->column_widths[col] = new_width;
    }
}


/* ── Inner width ───────────────────────────────────────────────────────── */

/** Returns the sum of all column widths plus inner vertical separators. */
static int compute_inner_width(const Table *table) {
    const ScTableData *data = table->table_data;
    int width = 0;
    for (size_t col = 0; col < data->column_count; col++) {
        width += table->column_widths[col];
        bool has_neighbor = (col + 1 < data->column_count);
        if (has_neighbor && table_has_vertical_separator_after(table, col)) {
            width++;
        }
    }
    return width;
}


/* ── Row heights ───────────────────────────────────────────────────────── */

/** Allocates and returns an array of visual heights for all data rows. */
static int *compute_row_heights(const Table *table) {
    size_t row_count = table->table_data->row_count;
    int *heights = malloc(row_count * sizeof(int));
    for (size_t row = 0; row < row_count; row++) {
        heights[row] = compute_row_height(table, row);
    }
    return heights;
}

/**
 * Returns the visual height of `row_index`: tallest cell content plus the
 * vertical padding, minimum 1.
 */
static int compute_row_height(const Table *table, size_t row_index) {
    int height = (int)tallest_cell_line_count(table, row_index)
                 + table->pad_vertical;
    return height < 1 ? 1 : height;
}

/** Returns the line count of the tallest non-spanning cell in `row_index`. */
static size_t tallest_cell_line_count(
    const Table *table, size_t row_index
) {
    const ScTableData *data = table->table_data;
    int horizontal_padding = table->pad_left + table->pad_right;

    size_t max_lines = 0;
    for (size_t col = 0; col < data->column_count; col++) {
        const ScCell *cell = &data->rows[row_index].cells[col];
        if (cell->col_span < 0) { continue; }
        if (cell->row_span < 0 || cell->row_span > 1) { continue; }

        int available_width = table->column_widths[col] - horizontal_padding;
        if (available_width < 0) { available_width = 0; }

        size_t lines = count_cell_lines(
            cell, &data->columns[col].opts, available_width
        );
        if (lines > max_lines) { max_lines = lines; }
    }
    return max_lines;
}
