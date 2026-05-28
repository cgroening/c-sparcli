#pragma once

#include "sparcli.h"


/* ── Render-time span / line types ───────────────────────────────────────── */

/** One styled text segment within a rendered line. */
typedef struct TSpan {
    /** UTF-8 string content of this span. */
    const char *text;

    /** Visual style (color, attributes) applied to this span. */
    ScTextStyle opts;
} TSpan;

/** One rendered line of cell content, composed of one or more styled spans. */
typedef struct TLine {
    /** Array of styled spans making up this line. */
    TSpan *spans;

    /** Number of spans in `spans`. */
    size_t count;

    /** Total visible column width of this line. */
    size_t visible_width;
} TLine;


/* ── Internal storage types ──────────────────────────────────────────────── */

/** Internal representation of a single table column. */
typedef struct TCol {
    /** Column header string; `NULL` if no header was set. */
    char *header;

    /** Display options for this column (alignment, width, wrap, …). */
    ScColOpts opts;

    /** Computed display width in terminal columns. */
    int width;
} TCol;

/** Internal representation of a single table row. */
typedef struct TRow {
    /** Array of cells; one entry per column. */
    ScCell *cells;

    /** Number of columns in `cells`. */
    size_t ncols;

    /** Row background color; `SC_ANSI_COLOR_NONE` if not set. */
    ScColor bg;
} TRow;

/** Heap-allocated table data created by `sc_table_new()`. */
typedef struct ScTableData {
    /** Array of column descriptors. */
    TCol *columns;

    /** Allocated slot count in `columns`. */
    size_t column_capacity;

    /** Number of columns in `columns`. */
    size_t column_count;

    /** Array of data rows. */
    TRow *rows;

    /** Allocated slot count in `rows`. */
    size_t row_capacity;

    /** Number of data rows in `rows`. */
    size_t row_count;

    /** Array of footer rows. */
    TRow *footer_rows;

    /** Allocated slot count in `footer_rows`. */
    size_t footer_rows_capacity;

    /** Number of footer rows in `footer_rows`. */
    size_t footer_row_count;
} ScTableData;


/* ── Border character sets ───────────────────────────────────────────────── */

/**
 * Box-drawing characters for one border style:
 * corners (`tl`/`tr`/`bl`/`br`), fill (`h`/`v`),
 * and T-junctions (`cross`/`t_top`/`t_bot`/`t_left`/`t_right`).
 */
typedef struct TableBorderCharacter {
    /** Top-left, top-right, bottom-left, bottom-right corner characters. */
    const char *tl, *tr, *bl, *br;

    /** Horizontal and vertical fill characters. */
    const char *h, *v;

    /** Cross, top-T, bottom-T, left-T, right-T junction characters. */
    const char *cross, *t_top, *t_bot, *t_left, *t_right;
} TableBorderCharacter;

/** Look-up table of box-drawing character sets, indexed by `SC_BORDER_*` style. */
static const TableBorderCharacter border_char_sets[] = {
    [SC_BORDER_NONE]    = { " "," "," "," ", " "," ", " "," "," "," "," " },
    [SC_BORDER_ASCII]   = { "+","+","+","+", "-","|", "+","+","+","+","+" },
    [SC_BORDER_SINGLE]  = { "┌","┐","└","┘", "─","│", "┼","┬","┴","├","┤" },
    [SC_BORDER_DOUBLE]  = { "╔","╗","╚","╝", "═","║", "╬","╦","╩","╠","╣" },
    [SC_BORDER_ROUNDED] = { "╭","╮","╰","╯", "─","│", "┼","┬","┴","├","┤" },
    [SC_BORDER_THICK]   = { "┏","┓","┗","┛", "━","┃", "╋","┳","┻","┣","┫" },
};


/* ── Render-time context types ───────────────────────────────────────────── */

/** Tracks the rendering state of an active rowspan across multiple rows. */
typedef struct RowSpan {
    /** Originating cell that started this rowspan; `NULL` if slot is unused. */
    const ScCell *cell;

    /** Total visual line height across all spanned rows (including separators). */
    int row_count;

    /** Number of visual lines already rendered for this span. */
    int line_offset;

    /** Vertical alignment for the spanned content. */
    ScVAlign valign;
} RowSpan;

/** Render-time working state for one `sc_table_print()` call. */
typedef struct Table {
    /** Source table data (columns, rows, footer rows). */
    const ScTableData *table_data;

    /** Rendering options passed to `sc_table_print()`. */
    ScTableOpts opts;

    /** Computed display width per column, in terminal columns. */
    int *column_widths;

    /** Per-column flag: `true` when the column is inside an active rowspan. */
    bool *has_rowspan;

    /** Per-column rowspan context; entry is active when `cell != NULL`. */
    RowSpan *row_span;

    /** Total inner width of the table (sum of column widths + separators). */
    int inner_width;

    /** Computed visual height per data row, in terminal lines. */
    int *row_heights;
} Table;
