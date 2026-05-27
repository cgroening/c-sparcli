#include "sparcli.h"
#include "internal.h"
#include <stddef.h>


/**
 * Maps each `ScTextAttribute` bitmask flag to its ANSI escape code string.
 *
 * Example: `SC_TEXT_ATTR_BOLD` (`0b0001`) maps to "\033[1m".
 */
static const struct {
    ScTextAttribute flag; const char *code;
} text_attr_map[] = {
    { SC_TEXT_ATTR_BOLD,   SC_ANSI_ESCAPE_CODE_BOLD      },
    { SC_TEXT_ATTR_DIM,    SC_ANSI_ESCAPE_CODE_DIM       },
    { SC_TEXT_ATTR_ITALIC, SC_ANSI_ESCAPE_CODE_ITALIC    },
    { SC_TEXT_ATTR_UNDER,  SC_ANSI_ESCAPE_CODE_UNDERLINE },
};

/**
 * Writes ANSI escape codes for all set flags in parameter `style` to stdout.
 *
 * Multiple flags may be combined:
 * `SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_ITALIC` emits both codes in sequence.
 *
 * @param style  Bitmask of `SC_TEXT_ATTR_*` flags.
 */
void sc_apply_style(ScTextAttribute attr) {
    if (!attr) { return; }

    size_t attr_count = sizeof(text_attr_map) / sizeof(text_attr_map[0]);
    for (size_t i = 0; i < attr_count; i++) {
        if (attr & text_attr_map[i].flag) {
            fputs(text_attr_map[i].code, stdout);
        }
    }
}
