#include "sparcli.h"
#include "internal.h"

#include <stdlib.h>


void sc_print(const char *text, ScTextStyle style) {
    if (!text) { return; }
    // Public entry: user strings cross the trust boundary here
    char *clean = sc_sanitize_copy(text, sc_allow_ansi());
    if (!clean) { return; }
    sc_print_raw(clean, style);
    free(clean);
}

void sc_println(const char *text, ScTextStyle style) {
    if (!text) { return; }
    sc_print(text, style);
    fputc('\n', sc_output_stream());
}

void sc_print_raw(const char *text, ScTextStyle style) {
    if (!text) { return; }
    sc_apply_style(style.attr);
    sc_apply_colors(style.fg, style.bg);
    fputs(text, sc_output_stream());
    fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());
}
