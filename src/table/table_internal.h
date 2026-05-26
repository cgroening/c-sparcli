#pragma once

#include "sparcli.h"


/* ── Render-time span / line types ───────────────────────────────────────── */

typedef struct {
    const char *text;
    ScTextStyle opts;
} TSpan;

typedef struct {
    TSpan *spans;
    size_t count;
    size_t vis_w;
} TLine;

/* ── Border character sets ───────────────────────────────────────────────── */

typedef struct {
    const char *tl, *tr, *bl, *br;
    const char *h, *v;
    const char *cross, *t_top, *t_bot, *t_left, *t_right;
} TBC;

static const TBC tbc[] = {
    [SC_BORDER_NONE]    = { " "," "," "," ", " "," ", " "," "," "," "," " },
    [SC_BORDER_ASCII]   = { "+","+","+","+", "-","|", "+","+","+","+","+" },
    [SC_BORDER_SINGLE]  = { "┌","┐","└","┘", "─","│", "┼","┬","┴","├","┤" },
    [SC_BORDER_DOUBLE]  = { "╔","╗","╚","╝", "═","║", "╬","╦","╩","╠","╣" },
    [SC_BORDER_ROUNDED] = { "╭","╮","╰","╯", "─","│", "┼","┬","┴","├","┤" },
    [SC_BORDER_THICK]   = { "┏","┓","┗","┛", "━","┃", "╋","┳","┻","┣","┫" },
};

/* ── Rowspan context ─────────────────────────────────────────────────────── */

typedef struct {
    const ScCell *cell;
    int           span_h;
    int           vis_offset;
    ScVAlign      valign;
} RSCtx;

/* ── Internal storage types ──────────────────────────────────────────────── */

typedef struct {
    char     *header;
    ScColOpts opts;
    int       width;
} TCol;

typedef struct {
    ScCell *cells;
    size_t  ncols;
    ScColor bg;
} TRow;

typedef struct ScTableData ScTableData;

struct ScTableData {
    TCol   *columns;
    size_t  column_capacity;
    size_t  column_count;
    TRow   *rows;
    size_t  row_capacity;
    size_t  row_count;
    TRow   *footer_rows;
    size_t  footer_rows_capacity;
    size_t  footer_row_count;
};

/* ── Render-time working struct ──────────────────────────────────────────── */

typedef struct {
    const ScTableData *table_data;
    ScTableOpts        opts;
    int               *column_widths;
    int               *is_rs;
    RSCtx             *rsc;
    int                inner_w;
    int               *row_heights;
} Table;
