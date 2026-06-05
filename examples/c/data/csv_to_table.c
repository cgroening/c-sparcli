// csv_to_table.c - read CSV with a header row and render it as a sparcli
// table (the serde reader feeding the output renderer).
//
// Run (after `make`):
//   make run-example EX=c/data/csv_to_table

#include <serde/sparcli_serde.h>
#include <sparcli.h>

#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *csv =
        "name,role,commits\n"
        "Ada,author,128\n"
        "Linus,maintainer,42\n";

    ScParseError err = { 0 };
    ScCsv *doc = sc_csv_parse(
        csv, strlen(csv), (ScCsvOpts){ .has_header = true }, &err
    );
    if (!doc) {
        sc_die(sc_parse_error_to_error(&err));
    }

    ScTableData *table = sc_table_new();
    size_t cols = sc_csv_cols(doc);
    for (size_t c = 0; c < cols; c++) {
        sc_table_add_column(table, sc_csv_header(doc, c), (ScColOpts){ 0 });
    }

    ScCell *cells = malloc(cols * sizeof *cells);
    if (!cells) {
        return 1;
    }
    size_t rows = sc_csv_data_rows(doc);
    for (size_t r = 0; r < rows; r++) {
        for (size_t c = 0; c < cols; c++) {
            cells[c] = sc_cell(sc_csv_at(doc, r + 1, c)); // +1 skips the header
        }
        sc_table_add_row(table, cells, cols);
    }
    free(cells);

    sc_table_print(
        table,
        (ScTableOpts){
            .border = { .type = SC_BORDER_ROUNDED },
            .header = {
                .row = true,
                .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                           SC_ANSI_COLOR_NONE },
            },
        }
    );

    sc_table_free(table);
    sc_csv_free(doc);
    return 0;
}
