/*
 * rule_kv.c - horizontal rules (section dividers) and key-value lists.
 *
 * Run (after `make`):
 *   make run-example EX=c/output/rule_kv
 */

#include <sparcli.h>

#include <stdio.h>


int main(void) {
    // Plain full-width rule, then one with a styled, centered title.
    sc_rule_str(NULL, (ScRuleOpts){ .type = SC_BORDER_SINGLE });
    sc_rule_str("Results", (ScRuleOpts){
        .type  = SC_BORDER_DOUBLE,
        .color = SC_ANSI_COLOR_CYAN,
        .title = {
            .style  = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                        SC_ANSI_COLOR_NONE },
            .halign = SC_ALIGN_CENTER,
            .pad    = 1,
        },
    });

    // Fixed width + placement, left-aligned title, vertical margin.
    sc_rule_str("left", (ScRuleOpts){
        .type   = SC_BORDER_THICK,
        .width  = 40,
        .halign = SC_ALIGN_LEFT,
        .title  = { .halign = SC_ALIGN_LEFT },
        .margin = { .top = 1, .bottom = 1 },
    });

    // Key-value list: keys padded to one column, values aligned behind them.
    ScKV *info = sc_kv_new((ScKVOpts){
        .key_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                       SC_ANSI_COLOR_NONE },
    });
    sc_kv_add(info, "Version", "1.2.0");
    sc_kv_add(info, "License", "MIT");
    sc_kv_add(info, "Platform", "macOS / Linux");
    sc_kv_print(info);
    sc_kv_free(info);
    printf("\n");

    // Fixed key column, custom separator, wrapped values with hanging indent.
    ScKV *details = sc_kv_new((ScKVOpts){
        .sep       = " : ",
        .key_width = 12,
        .width     = 56,
        .wrap_val  = 1,
        .val_style = { .attr = SC_TEXT_ATTR_DIM },
        .item_gap  = 1,
    });
    sc_kv_add(details, "Description",
              "A C11 library for styled terminal output: panels, tables, "
              "rules, lists, trees and interactive input widgets.");
    sc_kv_add(details, "Homepage", "https://github.com/example/sparcli");
    sc_kv_print(details);
    sc_kv_free(details);

    return 0;
}
