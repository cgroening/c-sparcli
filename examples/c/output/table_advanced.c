/*
 * table_advanced.c - tables: colspan/rowspan, stripes, per-column styles,
 * markup cells, word-wrap and row limits.
 *
 * Run (after `make`):
 *   make run-example EX=c/output/table_advanced
 */

#include <sparcli.h>

#include <stdio.h>


static void print_spans_table(void);
static void print_styled_table(void);
static void print_wrapped_table(void);


int main(void) {
    print_spans_table();
    printf("\n");
    print_styled_table();
    printf("\n");
    print_wrapped_table();
    return 0;
}

/** Colspan and rowspan: cells covering multiple grid positions. */
static void print_spans_table(void) {
    ScTableData *table = sc_table_new();
    sc_table_add_column(table, "Quarter", (ScColOpts){ 0 });
    sc_table_add_column(table, "Region",  (ScColOpts){ 0 });
    sc_table_add_column(table, "Revenue", (ScColOpts){
        .halign = SC_ALIGN_RIGHT,
    });

    // Rowspan: "Q1" spans two rows; the second row uses sc_row_skip().
    sc_table_add_row(table, (ScCell[]){
        sc_cell_rs("Q1", 2), sc_cell("Europe"), sc_cell("1.2 M"),
    }, 3);
    sc_table_add_row(table, (ScCell[]){
        sc_row_skip(), sc_cell("Americas"), sc_cell("2.4 M"),
    }, 3);

    // Colspan: one cell covering the remaining columns.
    sc_table_add_row(table, (ScCell[]){
        sc_cell("Q2"), sc_cell_csa("no data yet", 2, SC_ALIGN_CENTER),
        sc_cell_skip(),
    }, 3);

    sc_table_print(table, (ScTableOpts){
        .border = { .type = SC_BORDER_SINGLE },
        .header = { .row = true, .style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
        } },
        .title  = { .text = "Spans" },
    });
}

/** Stripes, header column, per-column styles and markup cells. */
static void print_styled_table(void) {
    ScTableData *table = sc_table_new();
    sc_table_add_column(table, "Check", (ScColOpts){
        // Per-column default style for cells without their own styling.
        .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
    });
    sc_table_add_column(table, "Result",   (ScColOpts){ 0 });
    sc_table_add_column(table, "Duration", (ScColOpts){
        .halign = SC_ALIGN_RIGHT,
        .style  = { .attr = SC_TEXT_ATTR_DIM },
    });

    // Markup cells own their parsed rich text; the table frees them.
    sc_table_add_row(table, (ScCell[]){
        sc_cell("build"), sc_cell_m("[green]✔ passed[/]"), sc_cell("41 s"),
    }, 3);
    sc_table_add_row(table, (ScCell[]){
        sc_cell("lint"), sc_cell_m("[green]✔ passed[/]"), sc_cell("3 s"),
    }, 3);
    sc_table_add_row(table, (ScCell[]){
        sc_cell("fuzz"), sc_cell_m("[red]✖ failed[/]"), sc_cell("122 s"),
    }, 3);

    sc_table_print(table, (ScTableOpts){
        .border = { .type = SC_BORDER_ROUNDED },
        // First column acts as a header column with its own background.
        .header = {
            .col    = true,
            .col_bg = sc_color_from_rgb(40, 40, 60),
        },
        .striped   = true,
        .stripe_bg = sc_color_from_rgb(30, 30, 30),
        .cell_pad  = { .left = 1, .right = 1 },
    });
}

/** Word-wrap, fixed widths, a row limit and a total table width. */
static void print_wrapped_table(void) {
    ScTableData *table = sc_table_new();
    sc_table_add_column(table, "Key", (ScColOpts){ .fixed_width = 10 });
    sc_table_add_column(table, "Description", (ScColOpts){
        .word_wrap = true,    // wrap long cells instead of truncating
    });

    sc_table_add_row(table, (ScCell[]){
        sc_cell("wrap"),
        sc_cell("Long cell content is word-wrapped to the column width "
                "instead of being cut off with an ellipsis."),
    }, 2);
    sc_table_add_row(table, (ScCell[]){
        sc_cell("limit"),
        sc_cell("max_rows truncates the body with an indicator row."),
    }, 2);
    sc_table_add_row(table, (ScCell[]){
        sc_cell("hidden"), sc_cell("This row falls beyond max_rows."),
    }, 2);

    sc_table_print(table, (ScTableOpts){
        .border      = { .type = SC_BORDER_SINGLE },
        .header      = { .row = true, .style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
        } },
        .total_width = 50,    // distribute across flexible columns
        .max_rows    = 2,     // truncate with "... N more rows"
    });
}
