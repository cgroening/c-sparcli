#pragma once

#include "core/sparcli_export.h"

#include <stdbool.h>
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
 * Namespace type for `SC_ANSI_COLOR_...` — groups all predefined ANSI colors
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
 * Combines padding and margin insets for layout.
 *
 * Both `padding` and `margin` are `ScEdges`, so they each have
 * top/right/bottom/left values. Zero-initialization means no padding/margin.
 */
typedef struct ScSpacing { ScEdges padding; ScEdges margin; } ScSpacing;

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
 * All visual properties of a component title: text, rendering, alignment,
 * padding and position. Used directly by ScRuleOpts (pos is ignored for
 * rules) and by panels/tables.
 */
typedef struct ScTitle {
    /** Title string; `NULL` = no title. */
    const char *text;

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
