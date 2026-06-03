/*
 * table_basic.c - tables: columns, header/footer rows, alignment, borders.
 *
 * Run (after `make`):
 *   make run-example EX=c/output/table_basic
 */

#include <sparcli.h>

#include <stdio.h>


int main(void) {
    // Build the data once; rendering options are passed at print time, so
    // the same table can be printed in different styles.
    ScTableData *table = sc_table_new();
    sc_table_add_column(table, "City",     (ScColOpts){ 0 });
    sc_table_add_column(table, "Country",  (ScColOpts){ 0 });
    sc_table_add_column(table, "Pop. (M)", (ScColOpts){
        .halign = SC_ALIGN_RIGHT,
    });

    // Data rows; the column headers above become the header row when
    // header.row is set at print time.
    sc_table_add_row(table, (ScCell[]){
        sc_cell("Tokyo"), sc_cell("Japan"), sc_cell("37.4"),
    }, 3);
    sc_table_add_row(table, (ScCell[]){
        sc_cell("Delhi"), sc_cell("India"), sc_cell("32.9"),
    }, 3);
    sc_table_add_row(table, (ScCell[]){
        sc_cell("Oslo"), sc_cell("Norway"), sc_cell("1.1"),
    }, 3);

    // Footer rows stick to the bottom and get the footer style.
    sc_table_add_footer_row(table, (ScCell[]){
        sc_cell("Total"), sc_cell("3 countries"), sc_cell("71.4"),
    }, 3);

    // 1. Default look: single border, bold header row.
    sc_table_print(table, (ScTableOpts){
        .border = { .type = SC_BORDER_SINGLE },
        .header = { .row = true, .style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
        } },
        .footer = { .style = { .attr = SC_TEXT_ATTR_DIM } },
    });
    printf("\n");

    // 2. Same data, different look: rounded colored border, title, padding.
    sc_table_print(table, (ScTableOpts){
        .border = {
            .type        = SC_BORDER_ROUNDED,
            .outer_color = SC_ANSI_COLOR_CYAN,
            .inner_color = SC_ANSI_COLOR_BLUE,
        },
        .header = {
            .row    = true,
            .style  = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                        SC_ANSI_COLOR_NONE },
        },
        .footer   = { .style = { .attr = SC_TEXT_ATTR_BOLD } },
        .title    = {
            .text  = "World cities",
            .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE,
                       SC_ANSI_COLOR_NONE },
        },
        .cell_pad = { .left = 1, .right = 1 },
    });
    printf("\n");

    // 3. Borderless: suppress the frame and inner separators.
    sc_table_print(table, (ScTableOpts){
        .border = { .no_outer = true, .no_inner_h = true },
        .header = { .row = true, .style = {
            SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_UNDER,
            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
        } },
        .cell_pad = { .right = 2 },
    });

    sc_table_free(table);
    return 0;
}
