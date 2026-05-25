#pragma once

#include "sparcli.h"         // IWYU pragma: export
#include "internal.h"        // IWYU pragma: export
#include "table_internal.h"  // IWYU pragma: export
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "table_print_init.c"


static void table_cleanup(Table *table);


void sc_table_print(const ScTableData *table_data, ScTableOpts opts) {
    if (!table_data->column_count) {
        return;
    }

    Table table = {0};
    table_init(&table, table_data, opts);
    table_render(&table);
    table_cleanup(&table);
}

static void table_cleanup(Table *table) {
    free(table->col_widths);
    free(table->row_heights);
    free(table->rsc);
    free(table->is_rs);
}
