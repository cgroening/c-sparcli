#pragma once

#include "sparcli_core.h"
#include "sparcli_text.h"
#include "sparcli_markup.h"
#include <stddef.h>

typedef enum { SC_CELL_STR, SC_CELL_TEXT, SC_CELL_MARKUP } ScCellKind;

typedef struct {
    ScCellKind  kind;
    const char *str;        /* not owned; used when kind == SC_CELL_STR */
    ScText     *text;       /* SC_CELL_TEXT: not owned. SC_CELL_MARKUP: owned by table */
    int         align_set;  /* 1 = overrides column alignment */
    ScHAlign     align;
    int         valign_set; /* 1 = overrides row valignment */
    ScVAlign    valign;
    int         colspan;    /* 0/1=normal, >1=spans N cols, -1=skip */
    int         rowspan;    /* 0/1=normal, >1=spans N rows, -1=skip */
} ScCell;

#define SC_CELL(s)            ((ScCell){ SC_CELL_STR,  (s), NULL, 0, 0, 0, 0, 0,     0 })
#define SC_CELL_A(s,ha,va)    ((ScCell){ SC_CELL_STR,  (s), NULL, 1,(ha), 1,(va), 0, 0 })
#define SC_CELL_T(t)          ((ScCell){ SC_CELL_TEXT, NULL, (t), 0, 0, 0, 0, 0,     0 })
#define SC_CELL_TA(t,ha,va)   ((ScCell){ SC_CELL_TEXT, NULL, (t), 1,(ha), 1,(va), 0, 0 })
#define SC_CELL_CS(s,cs)      ((ScCell){ SC_CELL_STR,  (s), NULL, 0, 0, 0, 0, (cs),  0 })
#define SC_CELL_CSA(s,cs,ha)  ((ScCell){ SC_CELL_STR,  (s), NULL, 1,(ha), 0, 0, (cs),  0 })
#define SC_CELL_TCS(t,cs)     ((ScCell){ SC_CELL_TEXT, NULL, (t), 0, 0, 0, 0, (cs),  0 })
#define SC_CELL_TCSA(t,cs,ha) ((ScCell){ SC_CELL_TEXT, NULL, (t), 1,(ha), 0, 0, (cs),  0 })
#define SC_CELL_SKIP          ((ScCell){ SC_CELL_STR,  "",  NULL, 0, 0, 0, 0, -1,    0 })
#define SC_CELL_RS(s,rs)      ((ScCell){ SC_CELL_STR,  (s), NULL, 0, 0, 0, 0, 0,  (rs) })
#define SC_CELL_TRS(t,rs)     ((ScCell){ SC_CELL_TEXT, NULL, (t), 0, 0, 0, 0, 0,  (rs) })
#define SC_ROW_SKIP           ((ScCell){ SC_CELL_STR,  "",  NULL, 0, 0, 0, 0, 0,    -1 })
/* SC_CELL_M: parse markup string; cell owns the ScText (freed by sc_table_free) */
#define SC_CELL_M(s)          ((ScCell){ SC_CELL_MARKUP, NULL, sc_markup_parse(s), 0, 0, 0, 0, 0, 0 })

typedef struct {
    int      min_w;
    int      max_w;
    int      fixed_w;
    ScHAlign  align;
    ScVAlign valign;
    int      wrap;   /* 1 = word-wrap instead of truncate */
    ScColor  bg;     /* SC_ANSI_COLOR_NONE = not set */
} ScColOpts;

typedef struct {
    ScBorderType style;
    ScColor       outer_color;
    ScColor       inner_color;
    ScColor       header_row_sep_color;
    ScColor       header_col_sep_color;
    int           no_outer;    /* 1 = suppress outer frame */
    int           no_inner_h;  /* 1 = suppress inner row separators */
    int           no_inner_v;  /* 1 = suppress inner col separators (except header col) */
} ScTableBorders;

typedef struct {
    ScTableBorders borders;
    int            header_row;
    int            header_col;
    ScColor        header_row_bg;
    ScColor        header_col_bg;
    ScTextStyle      header_opts;
    int            striped;
    ScColor        stripe_bg;
    ScColor        footer_row_bg;
    ScColor        footer_col_bg;
    ScTextStyle      footer_opts;
    const char    *title;
    ScTextStyle      title_opts;
    ScTitlePosition     title_pos;
    ScHAlign        title_align;
    int            title_pad;
    ScEdges        cell_pad;      /* inner cell padding (top/right/bottom/left) */
    ScEdges        margin;        /* outer table margin (top/right/bottom/left) */
    int            total_width;
    int            max_rows;
    int            rtl;
} ScTableOpts;

typedef struct ScTable ScTable;

ScTable *sc_table_new        (ScTableOpts opts);
void     sc_table_add_col    (ScTable *t, const char *header, ScColOpts col);
void     sc_table_add_row    (ScTable *t, ScCell *cells, size_t n);
void     sc_table_add_row_bg (ScTable *t, ScCell *cells, size_t n, ScColor bg);
void     sc_table_add_footer_row(ScTable *t, ScCell *cells, size_t n);
void     sc_table_print      (const ScTable *t);
void     sc_table_free       (ScTable *t);
