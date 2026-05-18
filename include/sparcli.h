#ifndef SPARCLI_H
#define SPARCLI_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ── ANSI escape codes (internal use) ──────────────────────────────────── */
#define SC_RESET   "\033[0m"
#define SC_BOLD    "\033[1m"
#define SC_DIM     "\033[2m"
#define SC_ITALIC  "\033[3m"
#define SC_UNDER   "\033[4m"

/* ── ScStyle ────────────────────────────────────────────────────────────── */

/* Each flag occupies exactly one bit so styles can be combined with |:
   SC_STYLE_BOLD | SC_STYLE_ITALIC = 0b0001 | 0b0010 = 0b0011 (unambiguous) */
typedef enum {
    SC_STYLE_NONE   = 0,
    SC_STYLE_BOLD   = 1 << 0,
    SC_STYLE_DIM    = 1 << 1,
    SC_STYLE_ITALIC = 1 << 2,
    SC_STYLE_UNDER  = 1 << 3,
} ScStyle;

/* ── ScColor ────────────────────────────────────────────────────────────── */

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

ScColor sc_rgb(uint8_t r, uint8_t g, uint8_t b);

/* ── ScOptions ──────────────────────────────────────────────────────────── */

typedef struct {
    ScStyle style;
    ScColor fg;
    ScColor bg;
} ScOptions;

void sc_print(const char *text, ScOptions opts);
void sc_println(const char *text, ScOptions opts);

/* ── ScText ─────────────────────────────────────────────────────────────── */

typedef struct {
    char     *text;
    ScOptions opts;
} ScSpan;

typedef struct {
    ScSpan *spans;
    size_t  count;
    size_t  cap;
} ScText;

ScText *sc_text_new(void);
void    sc_text_append(ScText *t, const char *text, ScOptions opts);
void    sc_text_free(ScText *t);
void    sc_print_text(const ScText *t);
size_t  sc_text_visible_width(const ScText *t);

/* ── Panels ─────────────────────────────────────────────────────────────── */

typedef enum {
    SC_BORDER_NONE,
    SC_BORDER_ASCII,
    SC_BORDER_SINGLE,
    SC_BORDER_DOUBLE,
    SC_BORDER_ROUNDED,
    SC_BORDER_THICK,
} ScBorderStyle;

typedef enum { SC_TITLE_TOP, SC_TITLE_BOTTOM } ScTitlePos;
typedef enum { SC_ALIGN_LEFT, SC_ALIGN_CENTER, SC_ALIGN_RIGHT } ScAlign;

typedef struct {
    ScBorderStyle border;
    ScColor       border_color;
    const char   *title;       /* NULL = kein Titel */
    ScOptions     title_opts;  /* empfohlen: SC_STYLE_BOLD */
    ScTitlePos    title_pos;
    ScAlign       title_align;
    int           pad_x;
    int           pad_y;
    int           width;       /* 0 = auto */
    int           title_pad;   /* spaces on each side of title text, default 1 */
} ScPanelOpts;

void sc_panel_str(const char *content, ScPanelOpts opts);
void sc_panel_text(const ScText *content, ScPanelOpts opts);

#endif /* SPARCLI_H */
