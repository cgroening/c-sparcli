#include "sparcli.h"
#include "internal.h"
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

void sc_print(const char *text, ScOptions opts) {
    apply_style(opts.style);
    sc_apply_colors(opts.fg, opts.bg);
    fputs(text, stdout);
    fputs(SC_RESET, stdout);
}

void sc_println(const char *text, ScOptions opts) {
    sc_print(text, opts);
    fputc('\n', stdout);
}
