#pragma once

#include "core/sparcli_export.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


SPARCLI_BEGIN_DECLS

/**
 * @defgroup sc_ansi_codes ANSI Escape Codes
 * @brief ANSI escape sequences for terminal text formatting.
 *
 * These macros define control sequences used to format text output
 * in terminals (e.g., bold, italic, underline, reset).
 *
 * @{
 */
#define SC_ANSI_ESCAPE_CODE_RESET     "\033[0m" /**< Resets all formatting */
#define SC_ANSI_ESCAPE_CODE_BOLD      "\033[1m" /**< Enables bold text */
#define SC_ANSI_ESCAPE_CODE_DIM       "\033[2m" /**< Enables dim (faint) text */
#define SC_ANSI_ESCAPE_CODE_ITALIC    "\033[3m" /**< Enables italic text */
#define SC_ANSI_ESCAPE_CODE_UNDERLINE "\033[4m" /**< Enables underlined text */
/** @} */

/**
 * Text attribute flags for terminal output (none, bold, etc.).
 *
 * Each flag occupies exactly one bit so styles can be combined with |.
 *
 * Example:
 * @code
 * SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_ITALIC  =  0b0001 | 0b0010  =  0b0011
 * @endcode
 */
typedef enum ScTextAttribute {
    SC_TEXT_ATTR_NONE   = 0,
    SC_TEXT_ATTR_BOLD   = 1 << 0,
    SC_TEXT_ATTR_DIM    = 1 << 1,
    SC_TEXT_ATTR_ITALIC = 1 << 2,
    SC_TEXT_ATTR_UNDER  = 1 << 3,
} ScTextAttribute;

/**
 * Namespace type for `ScTextAttribute` - groups all flags under dot notation.
 *
 * Copy `ScTextAttributeNs_` locally to get a short alias:
 *
 * @code
 * ScTextAttributeNs attr = ScTextAttributeNs_;
 * sc_print("hi", (ScTextStyle){ attr.BOLD | attr.ITALIC, ... });
 * // vs.
 * sc_print("hi", (ScTextStyle){
 *     SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_ITALIC, ... });
 * @endcode
 */
typedef struct ScTextAttributeNs {
    ScTextAttribute NONE, BOLD, DIM, ITALIC, UNDER;
} ScTextAttributeNs;

/**
 * Pre-initialized instance of `ScTextAttributeNs`.
 *
 * The trailing `_` distinguishes the instance from its type
 * `ScTextAttributeNs`. Declared `extern` so every translation unit sees
 * the same object (and FFI bindings can resolve the symbol).
 */
SPARCLI_EXPORT extern const ScTextAttributeNs ScTextAttributeNs_;

/**
 * Represents a terminal color, either as a named ANSI color or RGB.
 */
typedef struct ScColor {
    /** `0` = not set (zero-init); `-1` = RGB mode; `1`-`8` = ANSI color. */
    int index;

    /** RGB values (0-255), used when `index == -1`. */
    uint8_t r, g, b;
} ScColor;

/**
 * @defgroup sc_ansi_colors Predefined ANSI Colors
 * @brief Predefined ScColor instances for standard ANSI colors.
 *
 * Zero-initialized `ScColor` (i.e. `(ScColor){0}`) compares equal to
 * `SC_ANSI_COLOR_NONE`, so renderers treat any unset color field as
 * "no color" without the caller having to set it explicitly.
 *
 * @{
 */
#define SC_ANSI_COLOR_NONE    ((ScColor){ 0, 0, 0, 0 })
#define SC_ANSI_COLOR_BLACK   ((ScColor){ 1, 0, 0, 0 })
#define SC_ANSI_COLOR_RED     ((ScColor){ 2, 0, 0, 0 })
#define SC_ANSI_COLOR_GREEN   ((ScColor){ 3, 0, 0, 0 })
#define SC_ANSI_COLOR_YELLOW  ((ScColor){ 4, 0, 0, 0 })
#define SC_ANSI_COLOR_BLUE    ((ScColor){ 5, 0, 0, 0 })
#define SC_ANSI_COLOR_MAGENTA ((ScColor){ 6, 0, 0, 0 })
#define SC_ANSI_COLOR_CYAN    ((ScColor){ 7, 0, 0, 0 })
#define SC_ANSI_COLOR_WHITE   ((ScColor){ 8, 0, 0, 0 })
/** @} */

/**
 * Namespace type for `SC_ANSI_COLOR_...` - groups all predefined ANSI colors
 * under dot notation.
 *
 * Copy `ScAnsiColorNs_` locally to get a short alias:
 * @code
 * ScAnsiColorNs clr = ScAnsiColorNs_;
 * sc_print("hi", (ScTextStyle){ ..., clr.RED, clr.NONE });
 * @endcode
 */
typedef struct ScAnsiColorNs {
    ScColor NONE, BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE;
} ScAnsiColorNs;

/**
 * Pre-initialized instance of `ScAnsiColorNs`.
 *
 * The trailing `_` distinguishes the instance from its type
 * `ScAnsiColorNs`. Declared `extern` so every translation unit shares the
 * same object (and FFI bindings can resolve the symbol).
 */
SPARCLI_EXPORT extern const ScAnsiColorNs ScAnsiColorNs_;

/**
 * Combines text attributes and colors for terminal output.
 */
typedef struct ScTextStyle {
    /** Text attribute flags (bold, italic, …). */
    ScTextAttribute attr;

    /** Foreground color (ANSI index or RGB). */
    ScColor fg;

    /** Background color (ANSI index or RGB). */
    ScColor bg;
} ScTextStyle;

/**
 * Styles for border around components (none, ASCII, single, double, etc.).
 */
typedef enum ScBorderType {
    SC_BORDER_NONE,
    SC_BORDER_ASCII,
    SC_BORDER_SINGLE,
    SC_BORDER_DOUBLE,
    SC_BORDER_ROUNDED,
    SC_BORDER_THICK,
} ScBorderType;

/**
 * Position of the title in a component (top or bottom).
 */
typedef enum ScPosition {
    SC_POSITION_TOP,
    SC_POSITION_BOTTOM,
} ScPosition;

/**
 * Horizontal alignment options (left, center, right) for text and components.
 */
typedef enum ScHAlign {
    SC_ALIGN_LEFT, SC_ALIGN_CENTER, SC_ALIGN_RIGHT
} ScHAlign;

/**
 * Vertical alignment options (top, middle, bottom) for text and components.
 */
typedef enum ScVAlign {
    SC_VALIGN_TOP, SC_VALIGN_MIDDLE, SC_VALIGN_BOTTOM
} ScVAlign;

/**
 * Represents box-model insets (top, right, bottom, left) for layout
 * purposes. Zero-initialization means no inset.
 */
typedef struct ScEdges { int top; int right; int bottom; int left; } ScEdges;

/**
 * Groups the three visual properties of a border: character style,
 * foreground color, and background color.
 */
typedef struct ScBorderStyle {
    /** Border character set (none, ASCII, single, …). */
    ScBorderType type;

    /** Foreground color of border characters. */
    ScColor color;

    /** Background color of border characters; zero-init = none. */
    ScColor bg;
} ScBorderStyle;

/**
 * How a box resolves its width. Zero-init (`SC_WIDTH_DEFAULT`) leaves the
 * choice to the widget: list/choice widgets (select, fuzzy, confirm,
 * datepicker) fit their content, text-entry widgets span the full width.
 */
typedef enum ScWidthMode {
    SC_WIDTH_DEFAULT = 0, /**< Per-widget default (lists = content, text = full).
                               A non-zero `ScBoxStyle.width` means fixed. */
    SC_WIDTH_CONTENT,     /**< Fit the content, clamped to [min_width, max_width]. */
    SC_WIDTH_FIXED,       /**< Exactly `ScBoxStyle.width` columns. */
    SC_WIDTH_FULL,        /**< Full terminal width. */
} ScWidthMode;

/**
 * How far a background color extends across a widget's lines. The default
 * fills to the resolved widget width (so a row highlight is a full-width bar);
 * `SC_BG_EXTENT_TEXT` keeps the background hugging the text only.
 */
typedef enum ScBgExtent {
    SC_BG_EXTENT_WIDGET = 0, /**< Fill backgrounds to the widget width (default). */
    SC_BG_EXTENT_TEXT,       /**< Background only behind the text. */
} ScBgExtent;

/**
 * Shared "framed box" styling used by the input widgets: render a widget
 * inside a panel with an optional border, content background, inner padding,
 * outer margin and a width mode. Zero-init (`ScBoxStyle{0}`) means "no styling"
 * - the widget renders inline as before.
 *
 * The border, background, padding and width apply independently: `enabled`
 * requests a default (rounded) border, but a `bg`, `padding` or non-default
 * `width_mode` alone already routes the widget through the panel layout
 * *without* a border (a borderless background/width fill).
 *
 * The same struct is embedded in every `Sc*Opts` of the input layer and in
 * `ScInputTheme`, so box styling is configured uniformly across widgets and
 * can be themed once for the whole process.
 */
typedef struct ScBoxStyle {
    /** Draw a frame border. With a zero-init `border.type` this defaults to
        `SC_BORDER_ROUNDED`; an explicit `border.type` is used as-is. */
    bool enabled;

    /** Frame border (type/color/bg). A non-`SC_BORDER_NONE` type draws a border
        even when `enabled` is false; `SC_BORDER_NONE` = borderless. */
    ScBorderStyle border;

    /** Content-area background color; zero-init = none. Fills behind the
        widget's lines (to the width set by `bg_extent`), so list rows without
        their own background inherit it. Works with or without a border. */
    ScColor bg;

    /** Inner padding (top/right/bottom/left). An all-zero value selects the
        built-in default of one column left and right; set any edge to override
        it (use e.g. a non-default edge to opt out of the default entirely). */
    ScEdges padding;

    /** Outer margin around the frame; zero-init = none. */
    ScEdges margin;

    /** Width mode; zero-init = `SC_WIDTH_DEFAULT` (per-widget). */
    ScWidthMode width_mode;

    /** Fixed width in columns (used by `SC_WIDTH_FIXED`, and by
        `SC_WIDTH_DEFAULT` when non-zero). `0` = full terminal width for
        text-entry widgets / content width for lists. Also the inline field
        width for text-entry widgets. */
    int width;

    /** Lower width clamp for `SC_WIDTH_CONTENT`; `0` = none. */
    int min_width;

    /** Upper width clamp for `SC_WIDTH_CONTENT`; `0` = none. */
    int max_width;

    /** How far the background (and row highlights) extend; zero-init =
        `SC_BG_EXTENT_WIDGET` (full widget width). */
    ScBgExtent bg_extent;
} ScBoxStyle;

/**
 * All visual properties of a component title: text, rendering, alignment,
 * padding and position. Used directly by ScRuleOpts (pos is ignored for
 * rules) and by panels/tables.
 */
typedef struct ScTitle {
    /** Title string; `NULL` = no title. */
    const char *text;

    /**
     * Optional rich title (mixed styles). When non-`NULL` it overrides `text`
     * and `style`, and its visible width is used for layout. Currently honored
     * by panels (incl. boxed input prompts); rules/tables ignore it. Borrowed -
     * must outlive the render call.
     */
    const struct ScText *rich_text;

    /** Text style (bold, color, …) applied to the title. */
    ScTextStyle style;

    /** Horizontal placement of the title. */
    ScHAlign halign;

    /** Spaces on each side of the title text. */
    int pad;

    /** `SC_POSITION_TOP` / `SC_POSITION_BOTTOM`; unused for rules. */
    ScPosition pos;
} ScTitle;

/**
 * Per-widget ANSI passthrough mode for user-supplied strings.
 *
 * By default every string entering the library is sanitized: control
 * bytes (except `\n` and `\t`) and ANSI escape sequences are removed so
 * untrusted data cannot inject terminal escape codes. The zero-init
 * value inherits the process-wide setting (`sc_set_allow_ansi`); the
 * other values override it for one widget.
 */
typedef enum ScAnsiMode {
    /** Inherit the global `sc_set_allow_ansi` setting (the default). */
    SC_ANSI_MODE_DEFAULT = 0,

    /**
     * Pass well-formed ANSI escape sequences (CSI/OSC/DCS/…) through.
     * Stray control bytes are still removed; widths are computed
     * ANSI-aware so frames stay aligned.
     */
    SC_ANSI_MODE_ALLOW = 1,

    /** Always strip escape sequences, regardless of the global setting. */
    SC_ANSI_MODE_SANITIZE = 2,
} ScAnsiMode;


/**
 * Sets the process-wide default for ANSI passthrough in user strings.
 *
 * When `false` (the default), every string handed to the library has
 * ANSI escape sequences and control bytes (except `\n`, `\t`) stripped
 * at the API boundary - untrusted data cannot inject escape codes.
 * When `true`, well-formed escape sequences are preserved (stray
 * control bytes are still removed) and all width calculations skip
 * them, so borders and frames stay aligned.
 *
 * Per-widget opts can override this via their `ScAnsiMode ansi` field.
 * Thread-safe; intended as set-once configuration at startup.
 *
 * @param allow  `true` = pass ANSI sequences through; `false` = strip.
 */
SPARCLI_EXPORT void sc_set_allow_ansi(bool allow);

/**
 * Returns the current process-wide ANSI passthrough setting.
 *
 * @return  `true` when ANSI escape sequences are passed through.
 */
SPARCLI_EXPORT bool sc_allow_ansi(void);


/**
 * Creates an `ScColor` from 24-bit RGB values.
 *
 * The returned color has `index = -1`, which selects RGB mode when passed to
 * any function that accepts @ref ScColor.
 *
 * @param r  Red channel (0-255).
 * @param g  Green channel (0-255).
 * @param b  Blue channel (0-255).
 * @return   An `ScColor` with `index = -1` and the given RGB values.
 */
SPARCLI_EXPORT ScColor sc_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);

/**
 * Resolves a color name to an `ScColor`.
 *
 * Recognizes the eight ANSI color names (`"black"`, `"red"`, …, `"white"`)
 * and the named RGB palette (`SC_COLOR_*`, e.g. `"orange"`, `"accent"`,
 * `"error"`, `"bg_darken_1"`). The eight plain hue names always resolve to
 * the ANSI colors; the palette's own soft hues (`SC_COLOR_RED` etc.) are not
 * reachable by bare name here — use `#RRGGBB` or the `SC_COLOR_*` macro.
 *
 * @param name  Color name (case-sensitive); must not be `NULL`.
 * @param out   Receives the resolved color on success; must not be `NULL`.
 * @return      `true` when `name` matched, `false` otherwise (`*out` unset).
 */
SPARCLI_EXPORT bool sc_color_by_name(const char *name, ScColor *out);

/**
 * Length-delimited variant of @ref sc_color_by_name for non-terminated spans
 * (e.g. the markup parser). `name` need not be NUL-terminated.
 */
SPARCLI_EXPORT bool sc_color_by_name_n(
    const char *name, size_t length, ScColor *out
);

/**
 * Overrides the color a name resolves to at runtime.
 *
 * Recolors any name accepted by @ref sc_color_by_name (the eight ANSI names and
 * the named RGB palette, e.g. `"accent"`, `"accent_dark"`, `"error"`). The
 * override is honored by every name-based consumer — @ref sc_color_by_name and
 * thus markup (`[accent]`), the `sparcli` CLI (`--color accent`) and the args
 * parser (`SC_ARG_COLOR`) — and by widget defaults that resolve a palette name
 * (e.g. the fuzzy finder's accent). Pass a zero-init / `SC_ANSI_COLOR_NONE`
 * color to clear a single override.
 *
 * Note: this is the *name → color* palette, distinct from a widget's `accent`
 * field. To recolor the input widgets directly use
 * `sc_input_set_theme(&(ScInputTheme){ .accent = ... })`.
 *
 * Thread-safety: set-once before spawning threads (like the input theme and the
 * global logger); resolution itself is read-only.
 *
 * @param name   Color name (case-sensitive); must not be `NULL`.
 * @param color  New value (or `SC_ANSI_COLOR_NONE` to clear the override).
 * @return       `true` when `name` is a known color name, `false` otherwise.
 */
SPARCLI_EXPORT bool sc_palette_set(const char *name, ScColor color);

/**
 * Current effective value for `name` (override if set, else the built-in
 * default). Equivalent to @ref sc_color_by_name; provided for symmetry.
 */
SPARCLI_EXPORT bool sc_palette_get(const char *name, ScColor *out);

/** Clears every runtime palette override, restoring the built-in defaults. */
SPARCLI_EXPORT void sc_palette_reset(void);


/* ── Functional variants of the SC_ANSI_COLOR_* macros (FFI-friendly) ── */

/**
 * These eight functions return the predefined ANSI color constants as
 * regular function calls. Use them from bindings that cannot consume
 * the `SC_ANSI_COLOR_*` compound-literal macros (Python via ctypes/cffi,
 * Rust via bindgen for the macro values, etc.).
 *
 * The C-side macros remain available for native callers; they are
 * equivalent.
 */
SPARCLI_EXPORT ScColor sc_color_none(void);
SPARCLI_EXPORT ScColor sc_color_black(void);
SPARCLI_EXPORT ScColor sc_color_red(void);
SPARCLI_EXPORT ScColor sc_color_green(void);
SPARCLI_EXPORT ScColor sc_color_yellow(void);
SPARCLI_EXPORT ScColor sc_color_blue(void);
SPARCLI_EXPORT ScColor sc_color_magenta(void);
SPARCLI_EXPORT ScColor sc_color_cyan(void);
SPARCLI_EXPORT ScColor sc_color_white(void);

/**
 * Builds an `ScTextStyle` from its three components.
 *
 * Equivalent to the C99 compound literal
 * `(ScTextStyle){ attr, fg, bg }`, but usable from bindings that cannot
 * construct compound literals.
 */
SPARCLI_EXPORT ScTextStyle sc_text_style(
    ScTextAttribute attr, ScColor fg, ScColor bg
);

/**
 * Prints `text` to stdout with ANSI styling applied.
 *
 * Applies the attribute flags and foreground/background colors from `opts`
 * before the text, then always emits a reset escape (`\\033[0m`) afterwards
 * to prevent styling from leaking into subsequent output.
 *
 * @param raw_str  Null-terminated string to print. Must not be `NULL`.
 * @param style    Style and color options. Use `SC_TEXT_ATTR_NONE` to emit no
 *                 attribute codes. Use `SC_ANSI_COLOR_NONE` for fg/bg to emit
 *                 no color codes.
 *
 * @note The reset is always emitted, even when `opts` doesn't carry
 * any formatting.
 *
 * @code
 * sc_print("Error", (ScTextStyle){
 *     SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE
 * });
 * @endcode
 */
SPARCLI_EXPORT void sc_print(const char *raw_str, ScTextStyle style);

/**
 * Prints text to stdout with ANSI styling applied, followed by a newline.
 *
 * Equivalent to `sc_print` followed by `\\n`.
 *
 * @param raw_str  Null-terminated string to print. Must not be `NULL`.
 * @param style    Style and color options. See @ref sc_print for details.
 */
SPARCLI_EXPORT void sc_println(const char *raw_str, ScTextStyle style);

SPARCLI_END_DECLS
