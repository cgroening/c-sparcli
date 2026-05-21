#pragma once

#include <stdint.h>

/* ── ANSI escape codes (internal use) ──────────────────────────────────── */
#define SC_ANSI_ESCAPE_CODE_RESET     "\033[0m"
#define SC_ANSI_ESCAPE_CODE_BOLD      "\033[1m"
#define SC_ANSI_ESCAPE_CODE_DIM       "\033[2m"
#define SC_ANSI_ESCAPE_CODE_ITALIC    "\033[3m"
#define SC_ANSI_ESCAPE_CODE_UNDERLINE "\033[4m"

/* ── ScStyle ──────────────────────────────────────────────────────────── */

/* Each flag occupies exactly one bit so styles can be combined with |:
   SC_STYLE_BOLD | SC_STYLE_ITALIC = 0b0001 | 0b0010 = 0b0011 (unambiguous) */
typedef enum {
    SC_STYLE_NONE   = 0,
    SC_STYLE_BOLD   = 1 << 0,
    SC_STYLE_DIM    = 1 << 1,
    SC_STYLE_ITALIC = 1 << 2,
    SC_STYLE_UNDER  = 1 << 3,
} ScStyle;

/* ── ScColor ──────────────────────────────────────────────────────────── */

/* Named ANSI color (0-7) or 24-bit RGB. index -2 = not set, -1 = RGB mode. */
typedef struct {
    int     index;
    uint8_t r, g, b;
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

ScColor sc_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);

/* ── ScOptions ────────────────────────────────────────────────────────── */

typedef struct {
    ScStyle style;
    ScColor fg;
    ScColor bg;
} ScOptions;

void sc_print  (const char *text, ScOptions opts);
void sc_println(const char *text, ScOptions opts);

/* ── ScBorderStyle ────────────────────────────────────────────────────── */

typedef enum {
    SC_BORDER_NONE,
    SC_BORDER_ASCII,
    SC_BORDER_SINGLE,
    SC_BORDER_DOUBLE,
    SC_BORDER_ROUNDED,
    SC_BORDER_THICK,
} ScBorderStyle;

/* ── ScTitlePos / ScAlign / ScValign ──────────────────────────────────── */

typedef enum { SC_TITLE_TOP, SC_TITLE_BOTTOM }                  ScTitlePos;
typedef enum { SC_ALIGN_LEFT, SC_ALIGN_CENTER, SC_ALIGN_RIGHT } ScAlign;
typedef enum { SC_VALIGN_TOP, SC_VALIGN_MIDDLE, SC_VALIGN_BOTTOM } ScValign;
