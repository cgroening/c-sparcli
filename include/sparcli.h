#ifndef SPARCLI_H
#define SPARCLI_H

#include <stdint.h>
#include <stdio.h>

/* ANSI escape codes */
#define SC_RESET   "\033[0m"
#define SC_BOLD    "\033[1m"
#define SC_DIM     "\033[2m"
#define SC_ITALIC  "\033[3m"
#define SC_UNDER   "\033[4m"

/**
 * Style flags.
 *
 * Each value occupies exactly one bit (1, 2, 4, 8, ...) so that multiple styles
 * can be combined with | without collision, e.g.:
 * SC_STYLE_BOLD | SC_STYLE_ITALIC  =  0b0001 | 0b0010  =  0b0011 (unambiguous)
 */
typedef enum {
    SC_STYLE_NONE   = 0,
    SC_STYLE_BOLD   = 1 << 0,
    SC_STYLE_DIM    = 1 << 1,
    SC_STYLE_ITALIC = 1 << 2,
    SC_STYLE_UNDER  = 1 << 3,
} ScStyle;

/**
 * Color representation: either a named ANSI color (index 0-7) or a 24-bit RGB
 * color. Use the SC_COLOR_* macros for named colors and sc_rgb() for RGB.
 * index == -2 means "no color set".
 */
typedef struct {
    int     index;   /* -2: not set | -1: RGB | 0-7: named ANSI color */
    uint8_t r, g, b; /* used only when index == -1 */
} ScColor;

#define SC_COLOR_NONE    ((ScColor){ -2, 0, 0, 0 })
#define SC_COLOR_BLACK   ((ScColor){  0, 0, 0, 0 })
#define SC_COLOR_RED     ((ScColor){  1, 0, 0, 0 })
#define SC_COLOR_GREEN   ((ScColor){  2, 0, 0, 0 })
#define SC_COLOR_YELLOW  ((ScColor){  3, 0, 0, 0 })
#define SC_COLOR_BLUE    ((ScColor){  4, 0, 0, 0 })
#define SC_COLOR_MAGENTA ((ScColor){  5, 0, 0, 0 })
#define SC_COLOR_CYAN    ((ScColor){  6, 0, 0, 0 })
#define SC_COLOR_WHITE   ((ScColor){  7, 0, 0, 0 })

/* Create a 24-bit RGB color */
ScColor sc_rgb(uint8_t r, uint8_t g, uint8_t b);

/**
 * Options combining text style, foreground color, and background color.
 * Use SC_COLOR_NONE for fg or bg to leave that channel unstyled.
 */
typedef struct {
    ScStyle style;
    ScColor fg;
    ScColor bg;
} ScOptions;

/**
 * Prints text with the given options to stdout.
 * All styling is reset after printing.
 */
void sc_print(const char *text, ScOptions opts);

/**
 * Prints text with the given options followed by a newline to stdout.
 * All styling is reset after printing.
 */
void sc_println(const char *text, ScOptions opts);

#endif /* SPARCLI_H */
