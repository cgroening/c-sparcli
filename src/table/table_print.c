#include "sparcli.h"
#include "table/table_internal.h"

#include <stdlib.h>


/**
 * Box-drawing character lookup table indexed by `SC_BORDER_*` style.
 *
 * Declared `extern` in `table_internal.h` so every table TU shares the same
 * definition.
 */
const TableBorderCharacter border_char_sets[] = {
    [SC_BORDER_NONE]    = { " "," "," "," ", " "," ", " "," "," "," "," " },
    [SC_BORDER_ASCII]   = { "+","+","+","+", "-","|", "+","+","+","+","+" },
    [SC_BORDER_SINGLE]  = { "┌","┐","└","┘", "─","│", "┼","┬","┴","├","┤" },
    [SC_BORDER_DOUBLE]  = { "╔","╗","╚","╝", "═","║", "╬","╦","╩","╠","╣" },
    [SC_BORDER_ROUNDED] = { "╭","╮","╰","╯", "─","│", "┼","┬","┴","├","┤" },
    [SC_BORDER_THICK]   = { "┏","┓","┗","┛", "━","┃", "╋","┳","┻","┣","┫" },
};


static void table_cleanup(Table *table);


void sc_table_print(const ScTableData *table_data, ScTableOpts opts) {
    if (!table_data->column_count) { return; }

    Table table = { 0 };
    table_init(&table, table_data, opts);
    table_render(&table);
    table_cleanup(&table);
}


/** Frees the per-print arrays owned by `table`. */
static void table_cleanup(Table *table) {
    free(table->column_widths);
    free(table->row_heights);
    free(table->row_span);
    free(table->has_rowspan);
}
