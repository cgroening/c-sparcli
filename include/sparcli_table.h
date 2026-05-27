#pragma once

#include "sparcli_core.h"    // IWYU pragma: export
#include "sparcli_text.h"    // IWYU pragma: export
#include "sparcli_markup.h"  // IWYU pragma: export
#include <stddef.h>

typedef enum { SC_CELL_STR, SC_CELL_TEXT, SC_CELL_MARKUP } ScCellKind;

typedef struct {
    ScCellKind  kind;
    const char *str;        /* not owned; used when kind == SC_CELL_STR */
    ScText     *text;       /* SC_CELL_TEXT: not owned. SC_CELL_MARKUP: owned by table */
    bool        align_set;  /* overrides column alignment */
    ScHAlign     align;
    bool        valign_set; /* overrides row valignment */
    ScVAlign    valign;
    int         col_span;    /* 0/1=normal, >1=spans N cols, -1=skip */
    int         row_span;    /* 0/1=normal, >1=spans N rows, -1=skip */
} ScCell;

static inline ScCell sc_cell(const char *s) {
    return (ScCell){ .kind = SC_CELL_STR, .str = s };
}
static inline ScCell sc_cell_a(const char *s, ScHAlign ha, ScVAlign va) {
    return (ScCell){ .kind = SC_CELL_STR, .str = s,
                     .align_set = true, .align = ha,
                     .valign_set = true, .valign = va };
}
static inline ScCell sc_cell_t(ScText *t) {
    return (ScCell){ .kind = SC_CELL_TEXT, .text = t };
}
static inline ScCell sc_cell_ta(ScText *t, ScHAlign ha, ScVAlign va) {
    return (ScCell){ .kind = SC_CELL_TEXT, .text = t,
                     .align_set = true, .align = ha,
                     .valign_set = true, .valign = va };
}
static inline ScCell sc_cell_cs(const char *s, int cs) {
    return (ScCell){ .kind = SC_CELL_STR, .str = s, .col_span = cs };
}
static inline ScCell sc_cell_csa(const char *s, int cs, ScHAlign ha) {
    return (ScCell){ .kind = SC_CELL_STR, .str = s,
                     .align_set = true, .align = ha, .col_span = cs };
}
static inline ScCell sc_cell_tcs(ScText *t, int cs) {
    return (ScCell){ .kind = SC_CELL_TEXT, .text = t, .col_span = cs };
}
static inline ScCell sc_cell_tcsa(ScText *t, int cs, ScHAlign ha) {
    return (ScCell){ .kind = SC_CELL_TEXT, .text = t,
                     .align_set = true, .align = ha, .col_span = cs };
}
static inline ScCell sc_cell_skip(void) {
    return (ScCell){ .kind = SC_CELL_STR, .str = "", .col_span = -1 };
}
static inline ScCell sc_cell_rs(const char *s, int rs) {
    return (ScCell){ .kind = SC_CELL_STR, .str = s, .row_span = rs };
}
static inline ScCell sc_cell_trs(ScText *t, int rs) {
    return (ScCell){ .kind = SC_CELL_TEXT, .text = t, .row_span = rs };
}
static inline ScCell sc_row_skip(void) {
    return (ScCell){ .kind = SC_CELL_STR, .str = "", .row_span = -1 };
}
/* sc_cell_m: parses markup; cell owns the ScText (freed by sc_table_free) */
static inline ScCell sc_cell_m(const char *s) {
    return (ScCell){ .kind = SC_CELL_MARKUP, .text = sc_markup_parse(s) };
}

typedef struct {
    int      min_width;
    int      max_width;
    int      fixed_width;
    ScHAlign  halign;
    ScVAlign valign;
    bool     word_wrap;   /* word-wrap instead of truncate */
    ScColor  bg;     /* SC_ANSI_COLOR_NONE = not set */
} ScColOpts;

typedef struct {
    ScBorderType style;
    ScColor       outer_color;
    ScColor       inner_color;
    ScColor       header_row_sep_color;
    ScColor       header_col_sep_color;
    bool          no_outer;    /* suppress outer frame */
    bool          no_inner_h;  /* suppress inner row separators */
    bool          no_inner_v;  /* suppress inner col separators (except header col) */
} ScTableBorder;

typedef struct {
    bool        row;
    bool        col;
    ScColor     row_bg;
    ScColor     col_bg;
    ScTextStyle opts;
} ScTableHeader;

typedef struct {
    ScColor     row_bg;
    ScColor     col_bg;
    ScTextStyle opts;
} ScTableFooter;

typedef struct {
    ScTableBorder border;
    ScTableHeader  header;
    bool           striped;
    ScColor        stripe_bg;
    ScTableFooter  footer;
    ScTitle   title;
    ScEdges        cell_pad;      /* inner cell padding (top/right/bottom/left) */
    ScEdges        margin;        /* outer table margin (top/right/bottom/left) */
    int            total_width;
    int            max_rows;
    bool           rtl;
} ScTableOpts;

typedef struct ScTableData ScTableData;

ScTableData *sc_table_new(void);
void sc_table_add_column(ScTableData *table, const char *header, ScColOpts col);
void sc_table_add_row(ScTableData *table, ScCell *cells, size_t n);
void sc_table_add_row_bg(ScTableData *table, ScCell *cells, size_t n, ScColor bg);
void sc_table_add_footer_row(ScTableData *table, ScCell *cells, size_t n);
void sc_table_print(const ScTableData *table, ScTableOpts opts);
void sc_table_free(ScTableData *table);
