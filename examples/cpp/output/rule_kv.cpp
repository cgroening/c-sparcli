// rule_kv.cpp - horizontal rules and key-value lists (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/output/rule_kv

#include <sparcli.hpp>

using namespace sparcli;


int main() {
    // rule(opts) draws a plain divider; rule(title, opts) adds a label.
    rule({ .type = SC_BORDER_SINGLE });
    rule("Results", { .type = SC_BORDER_DOUBLE, .color = cyan(),
                      .title = { .style = style(SC_TEXT_ATTR_BOLD, cyan()),
                                 .halign = SC_ALIGN_CENTER, .pad = 1 } });

    // Fixed width, left placement, vertical margin.
    rule("left", { .type = SC_BORDER_THICK,
                   .title = { .halign = SC_ALIGN_LEFT },
                   .width = 40, .halign = SC_ALIGN_LEFT,
                   .margin = { 1, 0, 1, 0 } });

    // Key-value list: keys padded into one column.
    Kv info({ .key_style = style(SC_TEXT_ATTR_BOLD, cyan()) });
    info.add("Version", "1.2.0");
    info.add("License", "MIT");
    info.add("Platform", "macOS / Linux");
    info.print();
    println("");

    // Fixed key column, custom separator, wrapped values.
    Kv details({ .sep = " : ", .key_width = 12, .width = 56,
                 .item_gap = 1, .wrap_val = 1,
                 .val_style = style(SC_TEXT_ATTR_DIM) });
    details.add("Description",
                "A C11 library for styled terminal output: panels, tables, "
                "rules, lists, trees and interactive input widgets.");
    details.add("Homepage", "https://github.com/example/sparcli");
    details.print();

    return 0;
}
