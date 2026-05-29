#include "sparcli.h"
#include "internal.h"


typedef struct { int named; int rgb_prefix; } ColorLayer;


ScColor sc_ansi_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);
void sc_apply_colors(ScColor fg, ScColor bg);
static void apply_fg(ScColor c);
static void apply_bg(ScColor c);
static void apply_color_escape(ScColor clr, ColorLayer layer);


static const ColorLayer SC_ANSI_COLOR_LAYER_FOREGROUND = {3, 38};
static const ColorLayer SC_ANSI_COLOR_LAYER_BACKGROUND = {4, 48};


/**
 * Creates a color from RGB values and returns an `ScColor` struct.
 */
ScColor sc_ansi_color_from_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (ScColor){ -1, r, g, b };
}


/* ── Functional variants of the SC_ANSI_COLOR_* macros ──────────────────── */

/**
 * Single shared instance of `ScAnsiColorNs`. Declared `extern` in the
 * public header so all consumers (including FFI bindings) link to the
 * same object.
 */
const ScAnsiColorNs ScAnsiColorNs_ = {
    .NONE    = SC_ANSI_COLOR_NONE,
    .BLACK   = SC_ANSI_COLOR_BLACK,
    .RED     = SC_ANSI_COLOR_RED,
    .GREEN   = SC_ANSI_COLOR_GREEN,
    .YELLOW  = SC_ANSI_COLOR_YELLOW,
    .BLUE    = SC_ANSI_COLOR_BLUE,
    .MAGENTA = SC_ANSI_COLOR_MAGENTA,
    .CYAN    = SC_ANSI_COLOR_CYAN,
    .WHITE   = SC_ANSI_COLOR_WHITE,
};


ScColor sc_color_none(void)    { return SC_ANSI_COLOR_NONE; }
ScColor sc_color_black(void)   { return SC_ANSI_COLOR_BLACK; }
ScColor sc_color_red(void)     { return SC_ANSI_COLOR_RED; }
ScColor sc_color_green(void)   { return SC_ANSI_COLOR_GREEN; }
ScColor sc_color_yellow(void)  { return SC_ANSI_COLOR_YELLOW; }
ScColor sc_color_blue(void)    { return SC_ANSI_COLOR_BLUE; }
ScColor sc_color_magenta(void) { return SC_ANSI_COLOR_MAGENTA; }
ScColor sc_color_cyan(void)    { return SC_ANSI_COLOR_CYAN; }
ScColor sc_color_white(void)   { return SC_ANSI_COLOR_WHITE; }

ScTextStyle sc_text_style(
    ScTextAttribute attr, ScColor fg, ScColor bg
) {
    return (ScTextStyle){ attr, fg, bg };
}

/** Emits ANSI foreground and background escape codes for `fg` and `bg`. */
void sc_apply_colors(ScColor fg, ScColor bg) {
    apply_fg(fg);
    apply_bg(bg);
}

/** Emits the ANSI foreground color escape code for `clr`. */
static void apply_fg(ScColor clr) {
    apply_color_escape(clr, SC_ANSI_COLOR_LAYER_FOREGROUND);
}

/** Emits the ANSI background color escape code for `clr`. */
static void apply_bg(ScColor clr) {
    apply_color_escape(clr, SC_ANSI_COLOR_LAYER_BACKGROUND);
}

/**
 * Emits a single ANSI color escape code to stdout.
 *
 * Selects the escape format based on `clr.index`:
 * - `0` (`SC_ANSI_COLOR_NONE`, zero-init): no output, returns immediately.
 * - `-1` (24-bit RGB): emits `\033[{rgb_prefix};2;r;g;bm`.
 * - `1` to `8` (named color): emits `\033[{named}{index - 1}m`, i.e. the
 *   standard ANSI codes 30..37 (fg) / 40..47 (bg).
 *
 * @param clr    Color to emit.
 * @param layer  Prefix constants - use `SC_ANSI_COLOR_LAYER_FOREGROUND` or
 *               `SC_ANSI_COLOR_LAYER_BACKGROUND`.
 */
static void apply_color_escape(ScColor clr, ColorLayer layer) {
    int named = layer.named;
    int rgb_prefix = layer.rgb_prefix;

    if (clr.index == 0) {
        return;
    } else if (clr.index == -1) {
        fprintf(sc_output_stream(), "\033[%d;2;%d;%d;%dm", rgb_prefix, clr.r, clr.g, clr.b);
    } else {
        fprintf(sc_output_stream(), "\033[%d%dm", named, clr.index - 1);
    }
}
