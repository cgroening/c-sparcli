#include "sparcli.h"
#include "internal.h"


void sc_apply_style(ScTextAttribute style);
void sc_apply_colors(ScColor fg, ScColor bg);


void sc_print(const char *text, ScTextStyle style) {
    sc_apply_style(style.attr);
    sc_apply_colors(style.fg, style.bg);
    fputs(text, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

void sc_println(const char *text, ScTextStyle style) {
    sc_print(text, style);
    fputc('\n', stdout);
}
