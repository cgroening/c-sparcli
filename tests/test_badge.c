#include "sparcli.h"
#include <stdio.h>


static const ScTextStyle bold = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle beta_style = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLACK, SC_ANSI_COLOR_YELLOW
};
static const ScTextStyle stable_style = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE, SC_ANSI_COLOR_GREEN
};
static const ScTextStyle deprecated_style = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE, SC_ANSI_COLOR_RED
};
static const ScTextStyle release_style = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE
};


void test_badge(void) {
    printf("\n");

    /* ── 1. Default brackets ── */
    printf("--- Badge 1. Default brackets ---\n");
    sc_print_badge("INFO", (ScBadgeOpts){ 0 });
    fputc(' ', stdout);
    sc_print_badge("WARN", (ScBadgeOpts){ 0 });
    fputc(' ', stdout);
    sc_print_badge("ERROR", (ScBadgeOpts){ 0 });
    fputc('\n', stdout);

    printf("\n");

    /* ── 2. Custom caps ── */
    printf("--- Badge 2. Custom caps ---\n");
    sc_print_badge("v1.2.3", (ScBadgeOpts){
        .left_cap = "\xe2\x9d\xae",  /* ❮ U+276E */
        .right_cap = "\xe2\x9d\xaf", /* ❯ U+276F */
    });
    fputc(' ', stdout);
    sc_print_badge("main", (ScBadgeOpts){
        .left_cap = "(",
        .right_cap = ")",
    });
    fputc('\n', stdout);

    printf("\n");

    /* ── 3. Colored badges with padding ── */
    printf("--- Badge 3. Colored + padded ---\n");
    sc_print_badge("BETA", (ScBadgeOpts){
        .pad = 1,
        .text_style = beta_style,
    });
    fputc(' ', stdout);
    sc_print_badge("STABLE", (ScBadgeOpts){
        .pad = 1,
        .text_style = stable_style,
    });
    fputc(' ', stdout);
    sc_print_badge("DEPRECATED", (ScBadgeOpts){
        .pad = 1,
        .text_style = deprecated_style,
    });
    fputc('\n', stdout);

    printf("\n");

    /* ── 4. sc_text_append_badge inside ScText ── */
    printf("--- Badge 4. Inside ScText ---\n");
    {
        ScText *text = sc_text_new();
        sc_text_append(text, "Status: ", bold);
        sc_text_append_badge(text, "OK", (ScBadgeOpts){
            .pad = 1,
            .text_style = stable_style,
        });
        sc_text_append(text, "  Release: ", bold);
        sc_text_append_badge(text, "v2.0.0", (ScBadgeOpts){
            .left_cap = "(",
            .right_cap = ")",
            .text_style = release_style,
        });
        sc_print_text(text);
        fputc('\n', stdout);
        sc_text_free(text);
    }
}
