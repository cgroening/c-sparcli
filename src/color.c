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
 * - `-2` (`SC_ANSI_COLOR_NONE`): Mo output, returns immediately.
 * - `-1` (24-bit RGB): Emits `\033[{rgb_prefix};2;r;g;bm`.
 * - `0` to `7` (named color): Emits `\033[{named}{index}m`.
 *
 * @param clr    Color to emit.
 * @param layer  Prefix constants - use `SC_ANSI_COLOR_LAYER_FOREGROUND` or
 *               `SC_ANSI_COLOR_LAYER_BACKGROUND`.
 */
static void apply_color_escape(ScColor clr, ColorLayer layer) {
    int named = layer.named;
    int rgb_prefix = layer.rgb_prefix;

    if(clr.index == -2) {
        return;
    } else if(clr.index == -1) {
        fprintf(stdout, "\033[%d;2;%d;%d;%dm", rgb_prefix, clr.r, clr.g, clr.b);
    } else {
        fprintf(stdout, "\033[%d%dm", named, clr.index);
    }
}
