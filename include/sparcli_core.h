#pragma once

#include <stdint.h>

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
 * ScTextAttribute Enum - Text style flags for terminal output (none, bold, etc.).
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
 * The trailing `_` distinguishes the instance from its type `ScTextAttributeNs`.
 */
static const ScTextAttributeNs ScTextAttributeNs_ = {
    .NONE   = SC_TEXT_ATTR_NONE,
    .BOLD   = SC_TEXT_ATTR_BOLD,
    .DIM    = SC_TEXT_ATTR_DIM,
    .ITALIC = SC_TEXT_ATTR_ITALIC,
    .UNDER  = SC_TEXT_ATTR_UNDER
};

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
 * The trailing `_` distinguishes the instance from its type `ScAnsiColorNs`.
 */
static const ScAnsiColorNs ScAnsiColorNs_ = {
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

/**
 * ScTextStyle Struct - Combines text attributes and colors for terminal output.
 */
typedef struct {
    ScTextAttribute attr;  /**< Text attribute flags (bold, italic, etc.) */
    ScColor fg;            /**< Foreground color (ANSI index or RGB) */
    ScColor bg;            /**< Background color (ANSI index or RGB) */
} ScTextStyle;

/**
 * ScBorderType Enum - Styles for borders around components (none, ASCII,
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
 * ScTitlePosition Enum - Position of the title in a component (top or bottom).
 */
typedef enum { SC_TITLE_TOP, SC_TITLE_BOTTOM } ScTitlePosition;

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
 * Creates an `ScColor` from 24-bit RGB values.
 *
 * The returned color has `index = -1`, which selects RGB mode when passed to
 * any function that accepts @ref ScColor.
 *
 * @param r  Red channel (0–255).
 * @param g  Green channel (0–255).
 * @param b  Blue channel (0–255).
 * @return   An `ScColor` with `index = -1` and the given RGB values.
 */
ScColor sc_ansi_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);

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
void sc_print(const char *raw_str, ScTextStyle style);

/**
 * Prints text to stdout with ANSI styling applied, followed by a newline.
 *
 * Equivalent to `sc_print` followed by `\\n`.
 *
 * @param text  Null-terminated string to print. Must not be `NULL`.
 * @param opts  Style and color options. See @ref sc_print for details.
 */
void sc_println(const char *raw_str, ScTextStyle style);
