#include "sparcli.h"

static void apply_style(SpStyle style) {
    if (style & SP_STYLE_BOLD)   fputs(SP_BOLD,   stdout);
    if (style & SP_STYLE_DIM)    fputs(SP_DIM,    stdout);
    if (style & SP_STYLE_ITALIC) fputs(SP_ITALIC, stdout);
    if (style & SP_STYLE_UNDER)  fputs(SP_UNDER,  stdout);
}

void sc_print(const char *text, SpStyle style) {
    apply_style(style);
    fputs(text, stdout);
    if (style != SP_STYLE_NONE) fputs(SP_RESET, stdout);
}

void sc_println(const char *text, SpStyle style) {
    sc_print(text, style);
    fputc('\n', stdout);
}
