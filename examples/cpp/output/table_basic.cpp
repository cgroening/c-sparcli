// table_basic.cpp - tables with the RAII wrapper (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/output/table_basic

#include <sparcli.hpp>

#include <string>

using namespace sparcli;


int main() {
    // The Table owns its cell strings, so std::string temporaries are safe
    // and it frees itself on scope exit.
    Table table;
    table.add_column("City", { .halign = SC_ALIGN_LEFT });
    table.add_column("Country", { .halign = SC_ALIGN_LEFT });
    table.add_column("Pop. (M)", { .halign = SC_ALIGN_RIGHT });

    table.add_row({ "Tokyo", "Japan", "37.4" });
    table.add_row({ "Delhi", "India", "32.9" });
    table.add_row({ "Oslo", "Norway", "1.1" });
    table.add_footer_row({ "Total", "3 countries", "71.4" });

    // 1. Default look: single border, bold header row.
    table.print({ .border = { .type = SC_BORDER_SINGLE },
                  .header = { .row = true,
                              .style = style(SC_TEXT_ATTR_BOLD) },
                  .footer = { .style = style(SC_TEXT_ATTR_DIM) } });
    println("");

    // 2. Same data, rounded colored border and a title.
    table.print({ .border = { .type = SC_BORDER_ROUNDED,
                              .outer_color = cyan(), .inner_color = blue() },
                  .header = { .row = true,
                              .style = style(SC_TEXT_ATTR_BOLD, cyan()) },
                  .footer = { .style = style(SC_TEXT_ATTR_BOLD) },
                  .title  = { .text = "World cities",
                              .style = style(SC_TEXT_ATTR_BOLD) },
                  .cell_pad = { 0, 1, 0, 1 } });
    println("");

    // 3. Borderless.
    table.print({
        .border = { .no_outer = true, .no_inner_h = true },
        .header = { .row = true, .style = style(
            static_cast<TextAttribute>(SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_UNDER)) },
        .cell_pad = { 0, 2, 0, 0 } });

    return 0;
}
