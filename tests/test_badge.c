#include "sparcli.h"
#include <stdio.h>


void test_badge(void) {
    printf("\n");

    /* ── 1. Default brackets ── */
    printf("--- Badge 1. Default brackets ---\n");
    sc_print_badge("INFO",  (ScBadgeOpts){ 0 });
    fputc(' ', stdout);
    sc_print_badge("WARN",  (ScBadgeOpts){ 0 });
    fputc(' ', stdout);
    sc_print_badge("ERROR", (ScBadgeOpts){ 0 });
    fputc('\n', stdout);

    printf("\n");

    /* ── 2. Custom caps ── */
    printf("--- Badge 2. Custom caps ---\n");
    sc_print_badge("v1.2.3", (ScBadgeOpts){
        .left_cap  = "\xe2\x9d\xae",  /* ❮ U+276E */
        .right_cap = "\xe2\x9d\xaf",  /* ❯ U+276F */
    });
    fputc(' ', stdout);
    sc_print_badge("main", (ScBadgeOpts){
        .left_cap  = "(",
        .right_cap = ")",
    });
    fputc('\n', stdout);

    printf("\n");

    /* ── 3. Colored badges with padding ── */
    printf("--- Badge 3. Colored + padded ---\n");
    sc_print_badge("BETA", (ScBadgeOpts){
        .pad       = 1,
        .text_opts = { SC_STYLE_BOLD, SC_COLOR_BLACK, SC_COLOR_YELLOW },
    });
    fputc(' ', stdout);
    sc_print_badge("STABLE", (ScBadgeOpts){
        .pad       = 1,
        .text_opts = { SC_STYLE_BOLD, SC_COLOR_WHITE, SC_COLOR_GREEN },
    });
    fputc(' ', stdout);
    sc_print_badge("DEPRECATED", (ScBadgeOpts){
        .pad       = 1,
        .text_opts = { SC_STYLE_BOLD, SC_COLOR_WHITE, SC_COLOR_RED },
    });
    fputc('\n', stdout);

    printf("\n");

    /* ── 4. sc_text_append_badge inside ScText ── */
    printf("--- Badge 4. Inside ScText ---\n");
    {
        ScText *t = sc_text_new();
        sc_text_append(t, "Status: ",
                       (ScOptions){ SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE });
        sc_text_append_badge(t, "OK", (ScBadgeOpts){
            .pad       = 1,
            .text_opts = { SC_STYLE_BOLD, SC_COLOR_WHITE, SC_COLOR_GREEN },
        });
        sc_text_append(t, "  Release: ",
                       (ScOptions){ SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE });
        sc_text_append_badge(t, "v2.0.0", (ScBadgeOpts){
            .left_cap  = "(",
            .right_cap = ")",
            .text_opts = { SC_STYLE_NONE, SC_COLOR_CYAN, SC_COLOR_NONE },
        });
        sc_print_text(t);
        fputc('\n', stdout);
        sc_text_free(t);
    }
}
