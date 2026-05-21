#include "sparcli.h"
#include "internal.h"


void sc_apply_style(ScStyle style);
void sc_apply_colors(ScColor fg, ScColor bg);


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
    sc_apply_style(opts.style);
    sc_apply_colors(opts.fg, opts.bg);
    fputs(text, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
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
