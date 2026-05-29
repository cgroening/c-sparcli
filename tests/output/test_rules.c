#include "sparcli.h"
#include <stdio.h>


void test_rules(void) {
    printf("\n");

    /* ── 1. Line styles without color ── */
    sc_rule_str(NULL, (ScRuleOpts){
        .type = SC_BORDER_ASCII, .color = SC_ANSI_COLOR_NONE
    });
    sc_rule_str(NULL, (ScRuleOpts){
        .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE
    });
    sc_rule_str(NULL, (ScRuleOpts){
        .type = SC_BORDER_DOUBLE, .color = SC_ANSI_COLOR_NONE
    });
    sc_rule_str(NULL, (ScRuleOpts){
        .type = SC_BORDER_ROUNDED, .color = SC_ANSI_COLOR_NONE
    });
    sc_rule_str(NULL, (ScRuleOpts){
        .type = SC_BORDER_THICK, .color = SC_ANSI_COLOR_NONE
    });

    printf("\n");

    /* ── 2. Line styles with color ── */
    sc_rule_str(NULL, (ScRuleOpts){
        .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_CYAN
    });
    sc_rule_str(NULL, (ScRuleOpts){
        .type = SC_BORDER_DOUBLE, .color = SC_ANSI_COLOR_YELLOW
    });
    sc_rule_str(NULL, (ScRuleOpts){
        .type = SC_BORDER_THICK,
        .color = sc_color_from_rgb(180, 60, 60)
    });

    printf("\n");

    /* ── 3. Title: left / center / right alignment ── */
    sc_rule_str("Left", (ScRuleOpts){
        .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE,
        .title.style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        },
        .title.halign = SC_ALIGN_LEFT,
    });
    sc_rule_str("Middle", (ScRuleOpts){
        .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE,
        .title.style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        },
        .title.halign = SC_ALIGN_CENTER,
    });
    sc_rule_str("Right", (ScRuleOpts){
        .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE,
        .title.style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        },
        .title.halign = SC_ALIGN_RIGHT,
    });

    printf("\n");

    /* ── 4. Title with line and text color ── */
    sc_rule_str("Section A", (ScRuleOpts){
        .type = SC_BORDER_SINGLE,
        .color = SC_ANSI_COLOR_CYAN,
        .title.style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE
        },
        .title.halign = SC_ALIGN_CENTER,
        .title.pad = 2,
    });
    sc_rule_str("Warning", (ScRuleOpts){
        .type = SC_BORDER_DOUBLE,
        .color = SC_ANSI_COLOR_YELLOW,
        .title.style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE
        },
        .title.halign = SC_ALIGN_CENTER,
    });
    sc_rule_str("Error", (ScRuleOpts){
        .type = SC_BORDER_THICK,
        .color = sc_color_from_rgb(200, 50, 50),
        .title.style = {
            SC_TEXT_ATTR_BOLD,
            sc_color_from_rgb(200, 50, 50),
            SC_ANSI_COLOR_NONE
        },
        .title.halign = SC_ALIGN_CENTER,
        .title.pad = 2,
    });

    printf("\n");

    /* ── 5. Fixed width with terminal placement ── */
    sc_rule_str("left-aligned rule", (ScRuleOpts){
        .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE,
        .title.style = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        },
        .title.halign = SC_ALIGN_CENTER,
        .width = 40, .halign = SC_ALIGN_LEFT,
    });
    sc_rule_str("centered rule", (ScRuleOpts){
        .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE,
        .title.style = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        },
        .title.halign = SC_ALIGN_CENTER,
        .width = 40, .halign = SC_ALIGN_CENTER,
    });
    sc_rule_str("right-aligned rule", (ScRuleOpts){
        .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE,
        .title.style = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        },
        .title.halign = SC_ALIGN_CENTER,
        .width = 40, .halign = SC_ALIGN_RIGHT,
    });

    printf("\n");

    /* ── 6. Margin and pad_y ── */
    sc_rule_str("With Margin and Padding", (ScRuleOpts){
        .type = SC_BORDER_ROUNDED,
        .color = SC_ANSI_COLOR_GREEN,
        .title.style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE
        },
        .title.halign = SC_ALIGN_CENTER,
        .margin = {1, 8, 1, 8},
    });

    /* ── 7. ScText variant with multiple spans ── */
    {
        ScText *t = sc_text_new();
        sc_text_append(t, "Status: ", (ScTextStyle){
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        });
        sc_text_append(t, "OK", (ScTextStyle){
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE
        });
        sc_rule_text(t, (ScRuleOpts){
            .type = SC_BORDER_SINGLE,
            .color = SC_ANSI_COLOR_NONE,
            .title.halign = SC_ALIGN_CENTER,
        });
        sc_text_free(t);
    }
}
