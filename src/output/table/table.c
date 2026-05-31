#include "sparcli.h"
#include "output/table/table_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Initial capacity used when growing the column array. */
#define INITIAL_COLUMN_CAPACITY 4

/** Initial capacity used when growing a row array. */
#define INITIAL_ROW_CAPACITY 8


/**
 * Aliases the dynamic row array within `ScTableData` (either the data rows
 * block or the footer rows block), so the row-append helpers can work on
 * either without code duplication.
 */
typedef struct RowBlock {
    TRow **rows;
    size_t *count;
    size_t *capacity;
} RowBlock;


// Forward declarations indented to reflect call hierarchy
static void *dynarray_grow(
    void *array, size_t *capacity_in_out,
    size_t element_size, size_t initial_capacity
);

static void add_row(
    ScTableData *table_data, ScCell *cells, size_t cell_count,
    ScColor bg, bool to_footer
);
    static RowBlock get_row_block(ScTableData *table_data, bool to_footer);
    static bool grow_row_block(RowBlock *block);

static void free_row(TRow *row);
    static void free_row_array(TRow *rows, size_t count);


ScTableData *sc_table_new(void) {
    ScTableData *table_data = malloc(sizeof(ScTableData));
    if (!table_data) { return NULL; }
    table_data->columns = NULL;
    table_data->column_capacity = 0;
    table_data->column_count = 0;
    table_data->rows = NULL;
    table_data->row_capacity = 0;
    table_data->row_count = 0;
    table_data->footer_rows = NULL;
    table_data->footer_rows_capacity = 0;
    table_data->footer_row_count = 0;
    return table_data;
}

void sc_table_add_column(
    ScTableData *table_data, const char *header, ScColOpts col_opts
) {
    if (!table_data) { return; }

    if (table_data->column_count == table_data->column_capacity) {
        void *grown = dynarray_grow(
            table_data->columns, &table_data->column_capacity,
            sizeof(TCol), INITIAL_COLUMN_CAPACITY
        );
        if (!grown) { return; }
        table_data->columns = grown;
    }

    size_t slot = table_data->column_count;
    table_data->columns[slot].header = header ? strdup(header) : NULL;
    table_data->columns[slot].opts = col_opts;
    table_data->columns[slot].width = 0;
    table_data->column_count++;
}

void sc_table_add_row(ScTableData *table_data, ScCell *cells, size_t cell_count) {
    add_row(table_data, cells, cell_count, SC_ANSI_COLOR_NONE, false);
}

void sc_table_add_row_bg(
    ScTableData *table_data, ScCell *cells, size_t cell_count, ScColor bg
) {
    add_row(table_data, cells, cell_count, bg, false);
}

void sc_table_add_footer_row(
    ScTableData *table_data, ScCell *cells, size_t cell_count
) {
    add_row(table_data, cells, cell_count, SC_ANSI_COLOR_NONE, true);
}

/* ── Functional variants of the inline cell helpers ─────────────────────── */

ScCell sc_cell_from_str(const char *str) {
    return sc_cell(str);
}

ScCell sc_cell_str_aligned(
    const char *str, ScHAlign halign, ScVAlign valign
) {
    return sc_cell_a(str, halign, valign);
}

ScCell sc_cell_from_text(ScText *text) {
    return sc_cell_t(text);
}

ScCell sc_cell_text_aligned(
    ScText *text, ScHAlign halign, ScVAlign valign
) {
    return sc_cell_ta(text, halign, valign);
}

ScCell sc_cell_colspan(const char *str, int col_span) {
    return sc_cell_cs(str, col_span);
}

ScCell sc_cell_rowspan(const char *str, int row_span) {
    return sc_cell_rs(str, row_span);
}

ScCell sc_cell_skip_placeholder(void) {
    return sc_cell_skip();
}

ScCell sc_row_skip_placeholder(void) {
    return sc_row_skip();
}

ScCell sc_cell_from_markup(const char *markup) {
    return sc_cell_m(markup);
}


void sc_table_free(ScTableData *table_data) {
    if (!table_data) { return; }
    for (size_t i = 0; i < table_data->column_count; i++) {
        free(table_data->columns[i].header);
    }
    free(table_data->columns);
    free_row_array(table_data->rows, table_data->row_count);
    free_row_array(table_data->footer_rows, table_data->footer_row_count);
    free(table_data);
}


/**
 * Doubles the capacity of a dynamic array (or sets it to `initial_capacity`
 * when currently zero) and reallocates the storage.
 *
 * Returns the reallocated array, or `NULL` on allocation failure (the
 * existing storage is left untouched in that case, and `*capacity_in_out`
 * is not modified). Callers should treat a `NULL` return as "operation
 * failed; the table is unchanged".
 */
static void *dynarray_grow(
    void *array, size_t *capacity_in_out,
    size_t element_size, size_t initial_capacity
) {
    size_t new_capacity = *capacity_in_out == 0
        ? initial_capacity : *capacity_in_out * 2;
    void *tmp = realloc(array, new_capacity * element_size);
    if (!tmp) { return NULL; }
    *capacity_in_out = new_capacity;
    return tmp;
}

/**
 * Appends one row to either the data section or the footer section.
 *
 * Grows the target array on demand, then copies the cells and the row
 * background color. Cells beyond `cell_count` are filled with empty cells
 * so every row has exactly `column_count` cells.
 *
 * @param table_data  Table to modify.
 * @param cells       Array of cells; one entry per column.
 * @param cell_count  Number of elements in `cells`.
 * @param bg          Row background color; `SC_ANSI_COLOR_NONE` for none.
 * @param to_footer   `true` to append to the footer section,
 *                    `false` for the data section.
 */
static void add_row(
    ScTableData *table_data, ScCell *cells, size_t cell_count,
    ScColor bg, bool to_footer
) {
    if (!table_data) { return; }

    RowBlock block = get_row_block(table_data, to_footer);
    if (!grow_row_block(&block)) { return; }

    ScCell *row_cells = malloc(table_data->column_count * sizeof(ScCell));
    if (!row_cells) { return; }

    TRow *row = &(*block.rows)[(*block.count)++];
    row->ncols = table_data->column_count;
    row->bg = bg;
    row->cells = row_cells;

    for (size_t i = 0; i < table_data->column_count; i++) {
        row->cells[i] = (i < cell_count) ? cells[i] : sc_cell("");
    }
}

/**
 * Returns the row array, count and capacity pointers for the requested
 * section so `add_row` can operate on either block uniformly.
 */
static RowBlock get_row_block(ScTableData *table_data, bool to_footer) {
    if (to_footer) {
        return (RowBlock){
            .rows = &table_data->footer_rows,
            .count = &table_data->footer_row_count,
            .capacity = &table_data->footer_rows_capacity,
        };
    }
    return (RowBlock){
        .rows = &table_data->rows,
        .count = &table_data->row_count,
        .capacity = &table_data->row_capacity,
    };
}

/**
 * Grows the row array referenced by `block` when it has reached capacity.
 * Returns `true` on success; on allocation failure leaves `block` unchanged
 * and returns `false`.
 */
static bool grow_row_block(RowBlock *block) {
    if (*block->count != *block->capacity) { return true; }
    void *grown = dynarray_grow(
        *block->rows, block->capacity,
        sizeof(TRow), INITIAL_ROW_CAPACITY
    );
    if (!grown) { return false; }
    *block->rows = grown;
    return true;
}

/**
 * Releases the `ScText` of every `SC_CELL_MARKUP` cell and frees the
 * cells array. Does not free the `TRow` struct itself.
 */
static void free_row(TRow *row) {
    for (size_t i = 0; i < row->ncols; i++) {
        if (row->cells[i].kind == SC_CELL_MARKUP && row->cells[i].text) {
            sc_text_free(row->cells[i].text);
        }
    }
    free(row->cells);
}

/** Frees an array of rows and the array allocation itself. */
static void free_row_array(TRow *rows, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free_row(&rows[i]);
    }
    free(rows);
}
