#pragma once

#include "sparcli_export.h"

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
 * ScTextAttribute Enum - Text attributes flags for terminal output (none,
 * bold, etc.).
 *
 * Each flag occupies exactly one bit so styles can be combined with |.
 *
 * Example:
 * @code
 * SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_ITALIC  =  0b0001 | 0b0010  =  0b0011
 * @endcode
 */
typedef enum {
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
 * sc_print("hi", (ScTextStyle){ SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_ITALIC, ... });
 * @endcode
 */
typedef struct {
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
 * ScColor Struct - Represents a terminal color, either as a named ANSI color
 * or RGB.
 */
typedef struct {
    int index;  /**< -2 = not set; -1 = RGB mode; 0-7 = ANSI color index */
    uint8_t r, g, b;  /**< RGB values (0-255) used if index == -1 */
} ScColor;

/**
 * @defgroup sc_ansi_colors Predefined ANSI Colors
 * @brief Predefined ScColor instances for standard ANSI colors.
 *
 * @{
 */
#define SC_ANSI_COLOR_NONE    ((ScColor){ -2, 0, 0, 0 })
#define SC_ANSI_COLOR_BLACK   ((ScColor){  0, 0, 0, 0 })
#define SC_ANSI_COLOR_RED     ((ScColor){  1, 0, 0, 0 })
#define SC_ANSI_COLOR_GREEN   ((ScColor){  2, 0, 0, 0 })
#define SC_ANSI_COLOR_YELLOW  ((ScColor){  3, 0, 0, 0 })
#define SC_ANSI_COLOR_BLUE    ((ScColor){  4, 0, 0, 0 })
#define SC_ANSI_COLOR_MAGENTA ((ScColor){  5, 0, 0, 0 })
#define SC_ANSI_COLOR_CYAN    ((ScColor){  6, 0, 0, 0 })
#define SC_ANSI_COLOR_WHITE   ((ScColor){  7, 0, 0, 0 })
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
typedef struct {
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
 * ScTextStyle Struct - Combines text attributes and colors for terminal output.
 */
typedef struct {
    ScTextAttribute attr;  /**< Text attribute flags (bold, italic, etc.) */
    ScColor fg;            /**< Foreground color (ANSI index or RGB) */
    ScColor bg;            /**< Background color (ANSI index or RGB) */
} ScTextStyle;

/**
 * ScBorderType Enum - Styles for border around components (none, ASCII,
 * single, double, etc.).
 */
typedef enum {
    SC_BORDER_NONE,
    SC_BORDER_ASCII,
    SC_BORDER_SINGLE,
    SC_BORDER_DOUBLE,
    SC_BORDER_ROUNDED,
    SC_BORDER_THICK,
} ScBorderType;

/**
 * ScPosition Enum - Position of the title in a component (top or bottom).
 */
typedef enum {
    SC_POSITION_TOP, SC_POSITION_BOTTOM, SC_POSITION_LEFT, SC_POSITION_RIGHT
} ScPosition;

/**
 * ScHAlign Enum - Horizontal alignment options (left, center, right) for text
 * and components.
 */
typedef enum { SC_ALIGN_LEFT, SC_ALIGN_CENTER, SC_ALIGN_RIGHT } ScHAlign;

/**
 * ScVAlign Enum - Vertical alignment options (top, middle, bottom) for text
 * and components.
 */
typedef enum { SC_VALIGN_TOP, SC_VALIGN_MIDDLE, SC_VALIGN_BOTTOM } ScVAlign;

/**
 * ScEdges Struct - Represents box-model insets (top, right, bottom, left) for
 * layout purposes. Zero-initialization means no inset.
 */
typedef struct { int top; int right; int bottom; int left; } ScEdges;

/**
 * ScSpacing Struct - Combines padding and margin insets for layout.
 *
 * Both `padding` and `margin` are `ScEdges`, so they each have
 * top/right/bottom/left values. Zero-initialization means no padding/margin.
 */
typedef struct { ScEdges padding; ScEdges margin; } ScSpacing;

/**
 * ScBorderStyle Struct - Groups the three visual properties of a border:
 * character style, foreground color, and background color.
 */
typedef struct {
    ScBorderType type;   /**< Border character set (none, ASCII, single, …) */
    ScColor      color;  /**< Foreground color of border characters */
    ScColor      bg;     /**< Background color of border characters;
                              zero-init = none */
} ScBorderStyle;

/**
 * ScTitle Struct - All visual properties of a component title: text,
 * rendering, alignment, padding and position.
 * Used directly by ScRuleOpts (pos is ignored for rules) and by panels/tables.
 */
typedef struct {
    const char  *text;  /**< Title string; NULL = no title */
    ScTextStyle  style; /**< Text style (bold, color, …) applied to the title */
    ScHAlign     align; /**< Horizontal placement of the title */
    int          pad;   /**< Spaces on each side of the title text */
    ScPosition pos;  /**< SC_POSITION_TOP/SC_POSITION_BOTTOM; unused for rules */
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
SPARCLI_EXPORT ScColor sc_ansi_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);


/* ── Functional variants of the SC_ANSI_COLOR_* macros (FFI-friendly) ──── */

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
 * @param text  Null-terminated string to print. Must not be `NULL`.
 * @param opts  Style and color options. Use `SC_TEXT_ATTR_NONE` to emit no
 *              attribute codes. Use `SC_ANSI_COLOR_NONE` for fg/bg to emit no
 *              color codes.
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
 * @param text  Null-terminated string to print. Must not be `NULL`.
 * @param opts  Style and color options. See @ref sc_print for details.
 */
SPARCLI_EXPORT void sc_println(const char *raw_str, ScTextStyle style);

SPARCLI_END_DECLS
