#include "sparcli.h"
#include "internal.h"
#include <stddef.h>


/**
 * Single shared instance of `ScTextAttributeNs`. Declared `extern` in the
 * public header so all consumers (including FFI bindings) link to the
 * same object.
 */
const ScTextAttributeNs ScTextAttributeNs_ = {
    .NONE   = SC_TEXT_ATTR_NONE,
    .BOLD   = SC_TEXT_ATTR_BOLD,
    .DIM    = SC_TEXT_ATTR_DIM,
    .ITALIC = SC_TEXT_ATTR_ITALIC,
    .UNDER  = SC_TEXT_ATTR_UNDER,
    .STRIKE = SC_TEXT_ATTR_STRIKE,
};

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
    { SC_TEXT_ATTR_STRIKE, SC_ANSI_ESCAPE_CODE_STRIKE    },
};

/**
 * Writes ANSI escape codes for all set flags in `attr` to stdout.
 *
 * Multiple flags may be combined:
 * `SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_ITALIC` emits both codes in sequence.
 *
 * @param attr  Bitmask of `SC_TEXT_ATTR_*` flags.
 */
void sc_apply_style(ScTextAttribute attr) {
    if (!attr) { return; }

    size_t attr_count = sizeof(text_attr_map) / sizeof(text_attr_map[0]);
    for (size_t i = 0; i < attr_count; i++) {
        if (attr & text_attr_map[i].flag) {
            fputs(text_attr_map[i].code, sc_output_stream());
        }
    }
}
