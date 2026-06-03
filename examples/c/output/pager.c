/*
 * pager.c - route long output through $PAGER / `less -R`.
 *
 * Run (after `make`):
 *   make run-example EX=c/output/pager
 *
 * In a terminal the table opens in the pager (press `q` to quit). When the
 * output stream is not a TTY (piped/redirected) the pager is skipped and the
 * output is printed directly - the same code works in scripts.
 */

#include <sparcli.h>

#include <stdio.h>


/** Number of generated rows; enough to make paging worthwhile. */
#define ROW_COUNT 100


int main(void) {
    // Everything between begin and end goes through the pager.
    ScPager *pager = sc_pager_begin((ScPagerOpts){ 0 });

    sc_rule_str("100 rows", (ScRuleOpts){ .type = SC_BORDER_DOUBLE });

    ScTableData *table = sc_table_new();
    sc_table_add_column(table, "#",      (ScColOpts){
        .halign = SC_ALIGN_RIGHT,
    });
    sc_table_add_column(table, "Square", (ScColOpts){
        .halign = SC_ALIGN_RIGHT,
    });

    // Cell strings are borrowed until print, so the buffers must outlive
    // sc_table_print - one buffer per row, not one shared loop variable.
    static char numbers[ROW_COUNT][16];
    static char squares[ROW_COUNT][16];
    for (int i = 1; i <= ROW_COUNT; i++) {
        snprintf(numbers[i - 1], sizeof numbers[0], "%d", i);
        snprintf(squares[i - 1], sizeof squares[0], "%d", i * i);
        sc_table_add_row(table, (ScCell[]){
            sc_cell(numbers[i - 1]), sc_cell(squares[i - 1]),
        }, 2);
    }

    sc_table_print(table, (ScTableOpts){
        .border = { .type = SC_BORDER_SINGLE },
        .header = { .row = true, .style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
        } },
    });
    sc_table_free(table);

    // Always required, even when begin was a no-op; returns the pager's
    // exit status.
    int status = sc_pager_end(pager);
    printf("pager exit status: %d\n", status);
    return 0;
}
