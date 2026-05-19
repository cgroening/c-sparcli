#include "sparcli.h"
#include <stdio.h>


void test_rules(void) {
    printf("\n");

    /* ── 1. Line styles without color ── */
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_ASCII,   .color = SC_COLOR_NONE });
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_SINGLE,  .color = SC_COLOR_NONE });
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_DOUBLE,  .color = SC_COLOR_NONE });
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_ROUNDED, .color = SC_COLOR_NONE });
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_THICK,   .color = SC_COLOR_NONE });

    printf("\n");

    /* ── 2. Line styles with color ── */
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_SINGLE, .color = SC_COLOR_CYAN    });
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_DOUBLE, .color = SC_COLOR_YELLOW  });
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_THICK,  .color = sc_rgb(180,60,60) });

    printf("\n");

    /* ── 3. Title: left / center / right alignment ── */
    sc_rule_str("Links",  (ScRuleOpts){
        .style = SC_BORDER_SINGLE, .color = SC_COLOR_NONE,
        .title_opts  = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
        .title_align = SC_ALIGN_LEFT,
    });
    sc_rule_str("Mitte",  (ScRuleOpts){
        .style = SC_BORDER_SINGLE, .color = SC_COLOR_NONE,
        .title_opts  = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
    });
    sc_rule_str("Rechts", (ScRuleOpts){
        .style = SC_BORDER_SINGLE, .color = SC_COLOR_NONE,
        .title_opts  = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
        .title_align = SC_ALIGN_RIGHT,
    });

    printf("\n");

    /* ── 4. Title with line and text color ── */
    sc_rule_str("Section A", (ScRuleOpts){
        .style       = SC_BORDER_SINGLE,
        .color       = SC_COLOR_CYAN,
        .title_opts  = { SC_STYLE_BOLD, SC_COLOR_CYAN, SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
        .title_pad   = 2,
    });
    sc_rule_str("Warning", (ScRuleOpts){
        .style       = SC_BORDER_DOUBLE,
        .color       = SC_COLOR_YELLOW,
        .title_opts  = { SC_STYLE_BOLD, SC_COLOR_YELLOW, SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
    });
    sc_rule_str("Error", (ScRuleOpts){
        .style       = SC_BORDER_THICK,
        .color       = sc_rgb(200, 50, 50),
        .title_opts  = { SC_STYLE_BOLD, sc_rgb(200, 50, 50), SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
        .title_pad   = 2,
    });

    printf("\n");

    /* ── 5. Fixed width with terminal placement ── */
    sc_rule_str("left-aligned rule", (ScRuleOpts){
        .style = SC_BORDER_SINGLE, .color = SC_COLOR_NONE,
        .title_opts  = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
        .width = 40, .align = SC_ALIGN_LEFT,
    });
    sc_rule_str("centered rule", (ScRuleOpts){
        .style = SC_BORDER_SINGLE, .color = SC_COLOR_NONE,
        .title_opts  = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
        .width = 40, .align = SC_ALIGN_CENTER,
    });
    sc_rule_str("right-aligned rule", (ScRuleOpts){
        .style = SC_BORDER_SINGLE, .color = SC_COLOR_NONE,
        .title_opts  = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
        .width = 40, .align = SC_ALIGN_RIGHT,
    });

    printf("\n");

    /* ── 6. Margin and pad_y ── */
    sc_rule_str("mit Margin und pad_y", (ScRuleOpts){
        .style       = SC_BORDER_ROUNDED,
        .color       = SC_COLOR_GREEN,
        .title_opts  = { SC_STYLE_BOLD, SC_COLOR_GREEN, SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
        .margin      = 8,
        .pad_y       = 1,
    });

    /* ── 7. ScText variant with multiple spans ── */
    {
        ScText *t = sc_text_new();
        sc_text_append(t, "Status: ", (ScOptions){ SC_STYLE_NONE, SC_COLOR_NONE,  SC_COLOR_NONE });
        sc_text_append(t, "OK",       (ScOptions){ SC_STYLE_BOLD, SC_COLOR_GREEN, SC_COLOR_NONE });
        sc_rule_text(t, (ScRuleOpts){
            .style       = SC_BORDER_SINGLE,
            .color       = SC_COLOR_NONE,
            .title_align = SC_ALIGN_CENTER,
        });
        sc_text_free(t);
    }
}
