// table_advanced.cpp - colspan/rowspan, stripes, markup cells, per-cell
// options (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/output/table_advanced

#include <sparcli.hpp>

using namespace sparcli;


static void print_spans_table();
static void print_styled_table();


int main() {
    print_spans_table();
    println("");
    print_styled_table();
    return 0;
}

// Colspan/rowspan via per-cell chaining; skipped grid positions are omitted.
static void print_spans_table() {
    Table table;
    table.add_column("Quarter", {});
    table.add_column("Region", {});
    table.add_column("Revenue", { .halign = SC_ALIGN_RIGHT });

    // Rowspan: "Q1" covers two rows; the covered position is left out.
    table.add_row({ cell("Q1").rowspan(2), "Europe", "1.2 M" });
    table.add_row({ "Americas", "2.4 M" });

    // Colspan: one cell spanning the last two columns.
    table.add_row({ "Q2", cell("no data yet").colspan(2)
                              .align(SC_ALIGN_CENTER) });

    table.print({ .border = { .type = SC_BORDER_SINGLE },
                  .header = { .row = true, .style = style(SC_TEXT_ATTR_BOLD) },
                  .title  = { .text = "Spans" } });
}

// Stripes, a header column, per-row background and markup cells.
static void print_styled_table() {
    Table table;
    table.add_column("Check", {});
    table.add_column("Result", {});
    table.add_column("Duration", { .halign = SC_ALIGN_RIGHT });

    table.add_row({ "build", cell_markup("[green]✔ passed[/]"), "41 s" });
    table.add_row({ "lint", cell_markup("[green]✔ passed[/]"), "3 s" });
    // Per-row background highlights the failing check.
    table.add_row({ "fuzz", cell_markup("[red]✖ failed[/]"), "122 s" },
                  rgb(60, 30, 30));

    table.print({ .border = { .type = SC_BORDER_ROUNDED },
                  .header = { .col = true, .col_bg = rgb(40, 40, 60) },
                  .striped = true, .stripe_bg = rgb(30, 30, 30),
                  .cell_pad = { 0, 1, 0, 1 } });
}
