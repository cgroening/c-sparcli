#ifndef SPARCLI_H
#define SPARCLI_H

#include <stdio.h>

/* ANSI escape codes */
#define SC_RESET   "\033[0m"
#define SC_BOLD    "\033[1m"
#define SC_DIM     "\033[2m"
#define SC_ITALIC  "\033[3m"
#define SC_UNDER   "\033[4m"

/* Style flags (bitmask) */
#define SC_STYLE_NONE   0
#define SC_STYLE_BOLD   (1 << 0)
#define SC_STYLE_DIM    (1 << 1)
#define SC_STYLE_ITALIC (1 << 2)
#define SC_STYLE_UNDER  (1 << 3)

typedef unsigned int ScStyle;

/* Print text with style to stdout */
void sc_print(const char *text, ScStyle style);

/* Print text with style followed by newline */
void sc_println(const char *text, ScStyle style);

#endif /* SPARCLI_H */
