#include "sparcli.h"
#include "internal.h"
#include <stddef.h>


/**
 * Maps each `ScStyle` bitmask flag to its ANSI escape code string.
 *
 * Example: `SC_STYLE_BOLD` (`0b0001`) maps to "\033[1m".
 */
static const struct { ScStyle flag; const char *code; } style_map[] = {
    { SC_STYLE_BOLD,   SC_ANSI_ESCAPE_CODE_BOLD   },
    { SC_STYLE_DIM,    SC_ANSI_ESCAPE_CODE_DIM    },
    { SC_STYLE_ITALIC, SC_ANSI_ESCAPE_CODE_ITALIC },
    { SC_STYLE_UNDER,  SC_ANSI_ESCAPE_CODE_UNDERLINE  },
};

/**
 * Writes ANSI escape codes for all set flags in parameter `style` to stdout.
 *
 * Multiple flags may be combined: `SC_STYLE_BOLD | SC_STYLE_ITALIC` emits
 * both codes in sequence.
 *
 * @param style  Bitmask of `SC_STYLE_*` flags.
 */
void sc_apply_style(ScStyle style) {
    if (!style) return;
    for (size_t i = 0; i < sizeof(style_map) / sizeof(style_map[0]); i++) {
        if (style & style_map[i].flag)
            fputs(style_map[i].code, stdout);
    }
}
