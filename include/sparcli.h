#ifndef SPARCLI_H
#define SPARCLI_H

#include <stdio.h>

/* ANSI escape codes */
#define SP_RESET   "\033[0m"
#define SP_BOLD    "\033[1m"
#define SP_DIM     "\033[2m"
#define SP_ITALIC  "\033[3m"
#define SP_UNDER   "\033[4m"

/* Style flags (bitmask) */
#define SP_STYLE_NONE   0
#define SP_STYLE_BOLD   (1 << 0)
#define SP_STYLE_DIM    (1 << 1)
#define SP_STYLE_ITALIC (1 << 2)
#define SP_STYLE_UNDER  (1 << 3)

typedef unsigned int SpStyle;

/* Print text with style to stdout */
void sp_print(const char *text, SpStyle style);

/* Print text with style followed by newline */
void sp_println(const char *text, SpStyle style);

#endif /* SPARCLI_H */
