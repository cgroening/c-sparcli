#include "sparcli.h"
#include "table/table_internal.h"

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
    static void grow_row_block(RowBlock *block);

static void free_row(TRow *row);
    static void free_row_array(TRow *rows, size_t count);


ScTableData *sc_table_new(void) {
    ScTableData *table_data = malloc(sizeof(ScTableData));
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
    if (table_data->column_count == table_data->column_capacity) {
        table_data->columns = dynarray_grow(
            table_data->columns, &table_data->column_capacity,
            sizeof(TCol), INITIAL_COLUMN_CAPACITY
        );
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

void sc_table_free(ScTableData *table_data) {
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
 * when currently zero) and reallocates the storage. Aborts on allocation
 * failure.
 */
static void *dynarray_grow(
    void *array, size_t *capacity_in_out,
    size_t element_size, size_t initial_capacity
) {
    if (*capacity_in_out == 0) {
        *capacity_in_out = initial_capacity;
    } else {
        *capacity_in_out *= 2;
    }

    void *tmp = realloc(array, *capacity_in_out * element_size);
    if (!tmp) { abort(); }
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
    RowBlock block = get_row_block(table_data, to_footer);
    grow_row_block(&block);

    TRow *row = &(*block.rows)[(*block.count)++];
    row->ncols = table_data->column_count;
    row->bg = bg;
    row->cells = malloc(table_data->column_count * sizeof(ScCell));

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

/** Grows the row array referenced by `block` when it has reached capacity. */
static void grow_row_block(RowBlock *block) {
    if (*block->count == *block->capacity) {
        *block->rows = dynarray_grow(
            *block->rows, block->capacity,
            sizeof(TRow), INITIAL_ROW_CAPACITY
        );
    }
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
