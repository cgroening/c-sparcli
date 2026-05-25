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
    int         colspan;    /* 0/1=normal, >1=spans N cols, -1=skip */
    int         rowspan;    /* 0/1=normal, >1=spans N rows, -1=skip */
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
    return (ScCell){ .kind = SC_CELL_STR, .str = s, .colspan = cs };
}
static inline ScCell sc_cell_csa(const char *s, int cs, ScHAlign ha) {
    return (ScCell){ .kind = SC_CELL_STR, .str = s,
                     .align_set = true, .align = ha, .colspan = cs };
}
static inline ScCell sc_cell_tcs(ScText *t, int cs) {
    return (ScCell){ .kind = SC_CELL_TEXT, .text = t, .colspan = cs };
}
static inline ScCell sc_cell_tcsa(ScText *t, int cs, ScHAlign ha) {
    return (ScCell){ .kind = SC_CELL_TEXT, .text = t,
                     .align_set = true, .align = ha, .colspan = cs };
}
static inline ScCell sc_cell_skip(void) {
    return (ScCell){ .kind = SC_CELL_STR, .str = "", .colspan = -1 };
}
static inline ScCell sc_cell_rs(const char *s, int rs) {
    return (ScCell){ .kind = SC_CELL_STR, .str = s, .rowspan = rs };
}
static inline ScCell sc_cell_trs(ScText *t, int rs) {
    return (ScCell){ .kind = SC_CELL_TEXT, .text = t, .rowspan = rs };
}
static inline ScCell sc_row_skip(void) {
    return (ScCell){ .kind = SC_CELL_STR, .str = "", .rowspan = -1 };
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
} ScTableBorders;

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
    ScTableBorders borders;
    ScTableHeader  header;
    bool           striped;
    ScColor        stripe_bg;
    ScTableFooter  footer;
    ScTitle        title;
    ScEdges        cell_pad;      /* inner cell padding (top/right/bottom/left) */
    ScEdges        margin;        /* outer table margin (top/right/bottom/left) */
    int            total_width;
    int            max_rows;
    bool           rtl;
} ScTableOpts;

typedef struct ScTableData ScTableData;


/**
 * Allocates and initializes a new table.
 *
 * The returned pointer must be released with `sc_table_free` when no longer
 * needed. Layout and visual options are passed at render time via
 * `sc_table_print`.
 *
 * @return  Pointer to the newly allocated table.
 */
ScTableData *sc_table_new(void);

/**
 * Appends a column to the table.
 *
 * Must be called before any rows are added. The header string is copied
 * internally; the caller does not need to keep it alive.
 *
 * @param table   Table to modify. Must not be `NULL`.
 * @param header  Column header text, or `NULL` for no header.
 * @param col     Per-column layout options; see `ScColOpts`.
 */
void sc_table_add_column(ScTableData *table, const char *header, ScColOpts col);

/**
 * Appends a data row to the table.
 *
 * @param table  Table to modify. Must not be `NULL`.
 * @param cells  Array of cells; one entry per column.
 * @param n      Number of elements in `cells`.
 */
void sc_table_add_row(ScTableData *table, ScCell *cells, size_t n);

/**
 * Appends a data row with a custom background color to the table.
 *
 * @param table  Table to modify. Must not be `NULL`.
 * @param cells  Array of cells; one entry per column.
 * @param n      Number of elements in `cells`.
 * @param bg     Background color applied to every cell in this row.
 */
void sc_table_add_row_bg(ScTableData *table, ScCell *cells, size_t n, ScColor bg);

/**
 * Appends a row to the footer section of the table.
 *
 * Footer rows are rendered below all data rows, separated visually according
 * to `ScTableOpts.footer`.
 *
 * @param table  Table to modify. Must not be `NULL`.
 * @param cells  Array of cells; one entry per column.
 * @param n      Number of elements in `cells`.
 */
void sc_table_add_footer_row(ScTableData *table, ScCell *cells, size_t n);

/**
 * Renders the table to stdout.
 *
 * @param table  Table to print. Must not be `NULL`.
 * @param opts   Layout and visual options; see `ScTableOpts`.
 */
void sc_table_print(const ScTableData *table, ScTableOpts opts);

/**
 * Frees all resources owned by the table, including the table itself.
 *
 * @param table  Table to free. Must not be `NULL`.
 */
void sc_table_free(ScTableData *table);
