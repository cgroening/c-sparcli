#include "test_style.h"

#include <stdio.h>


void style_show(const char *caption, ScRendered *frame) {
    sc_println(caption, (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                                       SC_ANSI_COLOR_NONE });
    if (frame) {
        sc_pad_print(frame, (ScPadOpts){ .left = 2 });
        sc_rendered_free(frame);
    }
    printf("\n");
}

static void rule(const char *title) {
    sc_rule_str(title, (ScRuleOpts){
        .type        = SC_BORDER_DOUBLE,
        .title.style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
        .title.align = SC_ALIGN_CENTER,
    });
}

/**
 * Non-interactive style snapshot runner: renders every input widget in a
 * range of styles and prints the frames. Needs no TTY, so it is safe in CI.
 */
int main(void) {
    printf("\n");
    rule("Confirm");      style_confirm();    rule(NULL); printf("\n");
    rule("Text / Password"); style_text();    rule(NULL); printf("\n");
    rule("Select");       style_select();     rule(NULL); printf("\n");
    rule("Fuzzy Finder"); style_fuzzy();      rule(NULL); printf("\n");
    rule("Date Picker");  style_datepicker(); rule(NULL); printf("\n");
    return 0;
}
