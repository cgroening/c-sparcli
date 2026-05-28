#include "sparcli.h"
#include "internal.h"


void sc_apply_style(ScTextAttribute style);
void sc_apply_colors(ScColor fg, ScColor bg);


void sc_print(const char *text, ScTextStyle style) {
    if (!text) { return; }
    sc_apply_style(style.attr);
    sc_apply_colors(style.fg, style.bg);
    fputs(text, sc_output_stream());
    fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());
}

void sc_println(const char *text, ScTextStyle style) {
    if (!text) { return; }
    sc_print(text, style);
    fputc('\n', sc_output_stream());
}
