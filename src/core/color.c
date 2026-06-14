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
    /* ANSI colors (terminal palette). The _INIT (brace) forms are used because
     * a compound-literal color macro is not a constant initializer on MSVC. */
    { "black",   SC_ANSI_COLOR_BLACK_INIT   },
    { "red",     SC_ANSI_COLOR_RED_INIT     },
    { "green",   SC_ANSI_COLOR_GREEN_INIT   },
    { "yellow",  SC_ANSI_COLOR_YELLOW_INIT  },
    { "blue",    SC_ANSI_COLOR_BLUE_INIT    },
    { "magenta", SC_ANSI_COLOR_MAGENTA_INIT },
    { "cyan",    SC_ANSI_COLOR_CYAN_INIT    },
    { "white",   SC_ANSI_COLOR_WHITE_INIT   },
    /* Named RGB palette (SC_COLOR_*) */
    { "orange",            SC_COLOR_ORANGE_INIT },
    { "purple",            SC_COLOR_PURPLE_INIT },
    { "red_vivid",         SC_COLOR_RED_VIVID_INIT },
    { "orange_vivid",      SC_COLOR_ORANGE_VIVID_INIT },
    { "yellow_vivid",      SC_COLOR_YELLOW_VIVID_INIT },
    { "green_vivid",       SC_COLOR_GREEN_VIVID_INIT },
    { "cyan_vivid",        SC_COLOR_CYAN_VIVID_INIT },
    { "blue_vivid",        SC_COLOR_BLUE_VIVID_INIT },
    { "purple_vivid",      SC_COLOR_PURPLE_VIVID_INIT },
    { "magenta_vivid",     SC_COLOR_MAGENTA_VIVID_INIT },
    { "red_dark",          SC_COLOR_RED_DARK_INIT },
    { "orange_dark",       SC_COLOR_ORANGE_DARK_INIT },
    { "yellow_dark",       SC_COLOR_YELLOW_DARK_INIT },
    { "green_dark",        SC_COLOR_GREEN_DARK_INIT },
    { "cyan_dark",         SC_COLOR_CYAN_DARK_INIT },
    { "blue_dark",         SC_COLOR_BLUE_DARK_INIT },
    { "purple_dark",       SC_COLOR_PURPLE_DARK_INIT },
    { "magenta_dark",      SC_COLOR_MAGENTA_DARK_INIT },
    { "bg",                SC_COLOR_BG_INIT },
    { "bg_darken_1",       SC_COLOR_BG_DARKEN_1_INIT },
    { "bg_darken_2",       SC_COLOR_BG_DARKEN_2_INIT },
    { "bg_lighten_1",      SC_COLOR_BG_LIGHTEN_1_INIT },
    { "bg_lighten_2",      SC_COLOR_BG_LIGHTEN_2_INIT },
    { "bg_lighten_3",      SC_COLOR_BG_LIGHTEN_3_INIT },
    { "bg_selected",       SC_COLOR_BG_SELECTED_INIT },
    { "fg",                SC_COLOR_FG_INIT },
    { "fg_darken_1",       SC_COLOR_FG_DARKEN_1_INIT },
    { "fg_darken_2",       SC_COLOR_FG_DARKEN_2_INIT },
    { "fg_darken_3",       SC_COLOR_FG_DARKEN_3_INIT },
    { "fg_lighten_1",      SC_COLOR_FG_LIGHTEN_1_INIT },
    { "fg_lighten_2",      SC_COLOR_FG_LIGHTEN_2_INIT },
    { "accent",            SC_COLOR_ACCENT_INIT },
    { "accent_dim",        SC_COLOR_ACCENT_DIM_INIT },
    { "accent_darker",     SC_COLOR_ACCENT_DARKER_INIT },
    { "accent_dark",       SC_COLOR_ACCENT_DARK_INIT },
    { "accent_important",  SC_COLOR_ACCENT_IMPORTANT_INIT },
    { "enabled",           SC_COLOR_ENABLED_INIT },
    { "disabled",          SC_COLOR_DISABLED_INIT },
    { "disabled_dim",      SC_COLOR_DISABLED_DIM_INIT },
    { "error",             SC_COLOR_ERROR_INIT },
    { "warning",           SC_COLOR_WARNING_INIT },
    { "success",           SC_COLOR_SUCCESS_INIT },
    { "info",              SC_COLOR_INFO_INIT },
    { "hint",              SC_COLOR_HINT_INIT },
    { "unused",            SC_COLOR_UNUSED_INIT },
    { NULL,                SC_ANSI_COLOR_NONE_INIT },
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
    .NONE    = SC_ANSI_COLOR_NONE_INIT,
    .BLACK   = SC_ANSI_COLOR_BLACK_INIT,
    .RED     = SC_ANSI_COLOR_RED_INIT,
    .GREEN   = SC_ANSI_COLOR_GREEN_INIT,
    .YELLOW  = SC_ANSI_COLOR_YELLOW_INIT,
    .BLUE    = SC_ANSI_COLOR_BLUE_INIT,
    .MAGENTA = SC_ANSI_COLOR_MAGENTA_INIT,
    .CYAN    = SC_ANSI_COLOR_CYAN_INIT,
    .WHITE   = SC_ANSI_COLOR_WHITE_INIT,
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
