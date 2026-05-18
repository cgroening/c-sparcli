#ifndef SPARCLI_H
#define SPARCLI_H

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
 * Prints text with style to stdout.
 *
 * The style is reset to normal after printing, so it won't affect subsequent
 * output. If style is SC_STYLE_NONE, the text is printed without any styling.
 */
void sc_print(const char *text, ScStyle style);

/**
 * Prints text with style followed by newline to stdout.
 *
 * The style is reset to normal after printing, so it won't affect subsequent
 * output. If style is SC_STYLE_NONE, the text is printed without any styling.
 */
void sc_println(const char *text, ScStyle style);

#endif /* SPARCLI_H */
