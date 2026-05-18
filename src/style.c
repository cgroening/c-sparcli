#include "sparcli.h"
#include <stddef.h>

static const struct { ScStyle flag; const char *code; } style_map[] = {
    { SC_STYLE_BOLD,   SC_BOLD   },
    { SC_STYLE_DIM,    SC_DIM    },
    { SC_STYLE_ITALIC, SC_ITALIC },
    { SC_STYLE_UNDER,  SC_UNDER  },
};

static void apply_style(ScStyle style) {
    if (!style) return;
    for (size_t i = 0; i < sizeof(style_map) / sizeof(style_map[0]); i++) {
        if (style & style_map[i].flag)
            fputs(style_map[i].code, stdout);
    }
}

void sc_print(const char *text, ScStyle style) {
    apply_style(style);
    fputs(text, stdout);
    if (style != SC_STYLE_NONE) fputs(SC_RESET, stdout);
}

void sc_println(const char *text, ScStyle style) {
    sc_print(text, style);
    fputc('\n', stdout);
}
