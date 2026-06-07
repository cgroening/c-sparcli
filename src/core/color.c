#include "sparcli.h"
#include "internal.h"


typedef struct ColorLayer { int named; int rgb_prefix; } ColorLayer;


ScColor sc_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);
void sc_apply_colors(ScColor fg, ScColor bg);
static void apply_fg(ScColor c);
static void apply_bg(ScColor c);
static void apply_color_escape(ScColor clr, ColorLayer layer);


static const ColorLayer SC_ANSI_COLOR_LAYER_FOREGROUND = {3, 38};
static const ColorLayer SC_ANSI_COLOR_LAYER_BACKGROUND = {4, 48};


/**
 * Creates a color from RGB values and returns an `ScColor` struct.
 */
ScColor sc_color_from_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (ScColor){ -1, r, g, b };
}


/* ── Color-name resolution (shared by markup, CLI and the args parser) ──── */

/**
 * Canonical name -> color table. The eight plain hue names map to the ANSI
 * colors (terminal palette); every other entry is a named RGB palette color
 * (`SC_COLOR_*`). NULL-terminated.
 */
static const struct {
    const char *name;
    ScColor     color;
} sc_named_colors[] = {
    /* ANSI colors (terminal palette) */
    { "black",   SC_ANSI_COLOR_BLACK   },
    { "red",     SC_ANSI_COLOR_RED     },
    { "green",   SC_ANSI_COLOR_GREEN   },
    { "yellow",  SC_ANSI_COLOR_YELLOW  },
    { "blue",    SC_ANSI_COLOR_BLUE    },
    { "magenta", SC_ANSI_COLOR_MAGENTA },
    { "cyan",    SC_ANSI_COLOR_CYAN    },
    { "white",   SC_ANSI_COLOR_WHITE   },
    /* Named RGB palette (SC_COLOR_*) */
    { "orange",            SC_COLOR_ORANGE },
    { "purple",            SC_COLOR_PURPLE },
    { "red_vivid",         SC_COLOR_RED_VIVID },
    { "orange_vivid",      SC_COLOR_ORANGE_VIVID },
    { "yellow_vivid",      SC_COLOR_YELLOW_VIVID },
    { "green_vivid",       SC_COLOR_GREEN_VIVID },
    { "cyan_vivid",        SC_COLOR_CYAN_VIVID },
    { "blue_vivid",        SC_COLOR_BLUE_VIVID },
    { "purple_vivid",      SC_COLOR_PURPLE_VIVID },
    { "magenta_vivid",     SC_COLOR_MAGENTA_VIVID },
    { "red_dark",          SC_COLOR_RED_DARK },
    { "orange_dark",       SC_COLOR_ORANGE_DARK },
    { "yellow_dark",       SC_COLOR_YELLOW_DARK },
    { "green_dark",        SC_COLOR_GREEN_DARK },
    { "cyan_dark",         SC_COLOR_CYAN_DARK },
    { "blue_dark",         SC_COLOR_BLUE_DARK },
    { "purple_dark",       SC_COLOR_PURPLE_DARK },
    { "magenta_dark",      SC_COLOR_MAGENTA_DARK },
    { "bg",                SC_COLOR_BG },
    { "bg_darken_1",       SC_COLOR_BG_DARKEN_1 },
    { "bg_darken_2",       SC_COLOR_BG_DARKEN_2 },
    { "bg_lighten_1",      SC_COLOR_BG_LIGHTEN_1 },
    { "bg_lighten_2",      SC_COLOR_BG_LIGHTEN_2 },
    { "bg_lighten_3",      SC_COLOR_BG_LIGHTEN_3 },
    { "bg_selected",       SC_COLOR_BG_SELECTED },
    { "fg",                SC_COLOR_FG },
    { "fg_darken_1",       SC_COLOR_FG_DARKEN_1 },
    { "fg_darken_2",       SC_COLOR_FG_DARKEN_2 },
    { "fg_darken_3",       SC_COLOR_FG_DARKEN_3 },
    { "fg_lighten_1",      SC_COLOR_FG_LIGHTEN_1 },
    { "fg_lighten_2",      SC_COLOR_FG_LIGHTEN_2 },
    { "accent",            SC_COLOR_ACCENT },
    { "accent_dim",        SC_COLOR_ACCENT_DIM },
    { "accent_darker",     SC_COLOR_ACCENT_DARKER },
    { "accent_dark",       SC_COLOR_ACCENT_DARK },
    { "accent_important",  SC_COLOR_ACCENT_IMPORTANT },
    { "enabled",           SC_COLOR_ENABLED },
    { "disabled",          SC_COLOR_DISABLED },
    { "disabled_dim",      SC_COLOR_DISABLED_DIM },
    { "error",             SC_COLOR_ERROR },
    { "warning",           SC_COLOR_WARNING },
    { "success",           SC_COLOR_SUCCESS },
    { "info",              SC_COLOR_INFO },
    { "hint",              SC_COLOR_HINT },
    { "unused",            SC_COLOR_UNUSED },
    { NULL,                SC_ANSI_COLOR_NONE },
};

#define SC_N_NAMED (sizeof(sc_named_colors) / sizeof(sc_named_colors[0]))

/**
 * Runtime overrides, parallel to @ref sc_named_colors (one slot per entry,
 * incl. the trailing NULL sentinel, which is never used). A zero-init slot
 * (`index == 0` / `SC_ANSI_COLOR_NONE`) means "no override". Set-once before
 * spawning threads, like the input theme and the global logger; the resolver
 * itself only reads it.
 */
static ScColor g_palette_overrides[SC_N_NAMED];

/** Returns the table index for `name` (length-delimited), or -1 if unknown. */
static int sc_named_index(const char *name, size_t length) {
    for (int i = 0; sc_named_colors[i].name; i++) {
        if (strlen(sc_named_colors[i].name) == length
            && memcmp(name, sc_named_colors[i].name, length) == 0) {
            return i;
        }
    }
    return -1;
}

/** Length-delimited name lookup; see header for the resolution rules. */
bool sc_color_by_name_n(const char *name, size_t length, ScColor *out) {
    if (name == NULL || out == NULL) {
        return false;
    }
    int i = sc_named_index(name, length);
    if (i < 0) {
        return false;
    }
    *out = g_palette_overrides[i].index != 0
         ? g_palette_overrides[i]
         : sc_named_colors[i].color;
    return true;
}

bool sc_palette_set(const char *name, ScColor color) {
    if (name == NULL) {
        return false;
    }
    int i = sc_named_index(name, strlen(name));
    if (i < 0) {
        return false;
    }
    g_palette_overrides[i] = color;   /* index == 0 clears the override */
    return true;
}

bool sc_palette_get(const char *name, ScColor *out) {
    return sc_color_by_name(name, out);
}

void sc_palette_reset(void) {
    memset(g_palette_overrides, 0, sizeof g_palette_overrides);
}

/** NUL-terminated name lookup; delegates to @ref sc_color_by_name_n. */
bool sc_color_by_name(const char *name, ScColor *out) {
    if (name == NULL) {
        return false;
    }
    return sc_color_by_name_n(name, strlen(name), out);
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
        fprintf(
            sc_output_stream(), "\033[%d;2;%d;%d;%dm",
            rgb_prefix, clr.r, clr.g, clr.b
        );
    } else {
        fprintf(sc_output_stream(), "\033[%d%dm", named, clr.index - 1);
    }
}
