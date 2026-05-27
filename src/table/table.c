#include "sparcli.h"         // IWYU pragma: export
#include "internal.h"        // IWYU pragma: export
#include "table_internal.h"  // IWYU pragma: export
#include "table_print.c"     // IWYU pragma: export
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


typedef struct {
    TRow **rows;
    size_t *count;
    size_t *capacity;
} RowBlock;  // TODO: More meaningful name for this



/* ── Forward declarations ────────────────────────────────────────────────── */

ScTableData *sc_table_new(void);

void sc_table_add_column(ScTableData *table_data, const char *header, ScColOpts col);
    static void *dynarray_grow(
        void *array,
        size_t *current_capacity,
        size_t element_size,
        size_t initial_capacity
    );

void sc_table_add_row(ScTableData *table_data, ScCell *cells, size_t n);
void sc_table_add_row_bg(ScTableData *table_data, ScCell *cells, size_t n, ScColor bg);
void sc_table_add_footer_row(ScTableData *table_data, ScCell *cells, size_t n);
    static void add_row(
        ScTableData *table_data, ScCell *cells, size_t ncell, ScColor bg, bool to_footer
    );
        static RowBlock get_row_block(ScTableData *table_data, bool to_footer);
        static void grow_rows_array(RowBlock *row_block);

void sc_table_print(const ScTableData *table_data, ScTableOpts opts);


void sc_table_free(ScTableData *table_data);
    static void free_row(TRow *row);
    static void free_row_array(TRow *rows, size_t count);




static void    free_tlines(TLine *lines, size_t n);
static void    flush_tline(TLine **lines, size_t *cap, size_t *n,
                            TSpan *buf, size_t buf_n, size_t vis_w);
static TLine  *make_cell_lines(const ScCell *cell, size_t *out_n);
static size_t  cell_vis_width(const ScCell *cell);
static TLine  *wrap_one_tline(const TLine *src, int wrap_w, size_t *out_n);
static TLine  *wrap_cell_lines(const ScCell *cell, int wrap_w, size_t *out_n);
static void    print_ch(const char *s, ScColor fg);
static void    print_spaces_bg(int n, ScColor bg);
static void    print_span_bg(const char *text, ScTextStyle opts, ScColor cell_bg);




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
    ScTableData *table_data, const char *header, ScColOpts col
) {
    // Ensure there is enough capacity for the new column, growing if necessary
    if (table_data->column_count == table_data->column_capacity) {
        table_data->columns = dynarray_grow(
            table_data->columns, &table_data->column_capacity, sizeof(TCol), 4
        );
    }

    // Add the new column with the provided header and options
    table_data->columns[table_data->column_count].header = header ? strdup(header) : NULL;
    table_data->columns[table_data->column_count].opts   = col;
    table_data->columns[table_data->column_count].width  = 0;
    table_data->column_count++;
}


/**
 * @brief Grows a dynamic array by doubling its capacity.
 *
 * @param array            Pointer to the existing array or `NULL`.
 * @param current_capacity Current allocated capacity; updated in place.
 * @param element_size     Size of a single element in bytes.
 * @param initial_capacity Capacity to allocate when the array is empty.
 * @return                 Pointer to the reallocated array.
 */
static void *dynarray_grow(
    void *array,
    size_t *current_capacity,
    size_t element_size,
    size_t initial_capacity
) {
    // Double the capacity or set to initial_capacity if currently zero.
    if (*current_capacity == 0) {
        *current_capacity = initial_capacity;
    }
    else {
        *current_capacity *= 2;
    }

    // Reallocate the array to the new capacity, abort on failure
    void *tmp = realloc(array, *current_capacity * element_size);
    if (!tmp) {
        abort();
    }
    return tmp;
}


void sc_table_add_row(ScTableData *table_data, ScCell *cells, size_t n) {
    add_row(table_data, cells, n, SC_ANSI_COLOR_NONE, false);
}

void sc_table_add_row_bg(
    ScTableData *table_data, ScCell *cells, size_t n, ScColor bg
) {
    add_row(table_data, cells, n, bg, false);
}

void sc_table_add_footer_row(ScTableData *table_data, ScCell *cells, size_t n) {
    add_row(table_data, cells, n, SC_ANSI_COLOR_NONE, true);
}

/**
 * Appends a row to either the data or footer section of the table.
 *
 * Grows the target array if needed, then copies the cells and sets the
 * row background color. Shared implementation for `sc_table_add_row`,
 * `sc_table_add_row_bg` and `sc_table_add_footer_row`.
 *
 * @param table_data  Table to modify.
 * @param cells       Array of cells; one entry per column.
 * @param cell_count  Number of elements in `cells`.
 * @param bg          Row background color; `SC_ANSI_COLOR_NONE` for none.
 * @param to_footer   `true` to append to the footer section, `false` for
 *                    data rows.
 */
static void add_row(
    ScTableData *table_data,
    ScCell *cells,
    size_t cell_count,
    ScColor bg,
    bool to_footer
) {
    RowBlock row_block = get_row_block(table_data, to_footer);
    grow_rows_array(&row_block);

    // Determine the address of the new row and initialize it
    TRow *row = &(*row_block.rows)[(*row_block.count)++];
    row->ncols = table_data->column_count;
    row->bg    = bg;
    row->cells = malloc(table_data->column_count * sizeof(ScCell));

    // Copy provided cells into the new row, filling remaining cells with
    // empty content
    for (size_t i = 0; i < table_data->column_count; i++) {
        row->cells[i] = (i < cell_count) ? cells[i] : sc_cell("");
    }
}

/**
 * Returns a `RowBlock` struct containing pointers to the appropriate row array,
 * count and capacity based on whether the target is the main rows block or the
 * footer rows block.
 */
static RowBlock get_row_block(ScTableData *table_data, bool to_footer) {
    if(to_footer) {
        return (RowBlock){
            .rows = &table_data->footer_rows,
            .count = &table_data->footer_row_count,
            .capacity = &table_data->footer_rows_capacity
        };
    } else {
        return (RowBlock){
            .rows = &table_data->rows,
            .count = &table_data->row_count,
            .capacity = &table_data->row_capacity
        };
    }
}

/**
 * Checks if the row array has reached its capacity and grows it if necessary.
 */
static void grow_rows_array(RowBlock *row_block) {
    if (*row_block->count == *row_block->capacity) {
        *row_block->rows = dynarray_grow(
            *row_block->rows, row_block->capacity, sizeof(TRow), 8
        );
    }
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
 * Frees all resources owned by a single row.
 *
 * Releases the `ScText` of every `SC_CELL_MARKUP` cell, then frees the
 * cells array. Does not free the `TRow` struct itself.
 *
 * @param row Pointer to the row to free.
 */
static void free_row(TRow *row) {
    for (size_t j = 0; j < row->ncols; j++) {
        if (row->cells[j].kind == SC_CELL_MARKUP && row->cells[j].text) {
            sc_text_free(row->cells[j].text);
        }
    }
    free(row->cells);
}

/**
 * Frees an array of rows and the array itself.
 *
 * Calls `free_row` on each element, then frees the array pointer.
 *
 * @param rows  Pointer to the row array; may be `NULL`.
 * @param count Number of rows in the array.
 */
static void free_row_array(TRow *rows, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free_row(&rows[i]);
    }
    free(rows);
}
