#include "sparcli.h"
#include "internal.h"
#include <stddef.h>


/**
 * Maps each `ScStyle` bitmask flag to its ANSI escape code string.
 *
 * Example: `SC_STYLE_BOLD` (`0b0001`) maps to "\033[1m".
 */
static const struct { ScStyle flag; const char *code; } style_map[] = {
    { SC_STYLE_BOLD,   SC_BOLD   },
    { SC_STYLE_DIM,    SC_DIM    },
    { SC_STYLE_ITALIC, SC_ITALIC },
    { SC_STYLE_UNDER,  SC_UNDER  },
};

/**
 * Writes ANSI escape codes for all set flags in parameter `style` to stdout.
 *
 * Multiple flags may be combined: `SC_STYLE_BOLD | SC_STYLE_ITALIC` emits
 * both codes in sequence.
 *
 * @param style  Bitmask of `SC_STYLE_*` flags.
 */
static void apply_style(ScStyle style) {
    if (!style) return;
    for (size_t i = 0; i < sizeof(style_map) / sizeof(style_map[0]); i++) {
        if (style & style_map[i].flag)
            fputs(style_map[i].code, stdout);
    }
}

/**
 * Prints the given `text` to stdout with ANSI styling applied.
 *
 * Applies the style flags and foreground/background colors from `opts` before
 * the `text`, then always emits a reset escape (`\033[0m`) afterwards to
 * prevent styling from leaking into subsequent output.
 *
 * @param text  Null-terminated string to print. Must not be `NULL`.
 * @param opts  Style and color options. Use `SC_STYLE_NONE` to emit no style
 *              codes. Use `SC_COLOR_NONE` for fg/bg to emit no color codes.
 *
 * @note The reset is always emitted, even when `opts` carries no formatting.
 *
 * Example:
 * @code
 * sc_print("Error", (ScOptions){ SC_STYLE_BOLD, SC_COLOR_RED, SC_COLOR_NONE });
 * @endcode
 */
void sc_print(const char *text, ScOptions opts) {
    apply_style(opts.style);
    sc_apply_colors(opts.fg, opts.bg);
    fputs(text, stdout);
    fputs(SC_RESET, stdout);
}

/**
 * Prints the given `text` to stdout with ANSI styling applied, followed by
 * a newline.
 *
 * Equivalent to `sc_print()` with a trailing `\n`.
 *
 * @param text  Null-terminated string to print. Must not be `NULL`.
 * @param opts  Style and color options. See `sc_print()` for details.
 */
void sc_println(const char *text, ScOptions opts) {
    sc_print(text, opts);
    fputc('\n', stdout);
}
