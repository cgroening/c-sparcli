#pragma once

#include "sparcli.h"

#include <stdbool.h>
#include <stddef.h>


/* ── Render-time span / line types ───────────────────────────────────────── */

/** One styled text segment within a rendered line. */
typedef struct TSpan {
    /** UTF-8 string content; owned by the surrounding `TLine`. */
    const char *text;

    /** Visual style (color, attributes) applied to this span. */
    ScTextStyle style;
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
    /** Column header string; `NULL` when no header was set. */
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

    /** Row background color; `SC_ANSI_COLOR_NONE` when not set. */
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
 * Box-drawing characters for one border style: corners (`tl`/`tr`/`bl`/`br`),
 * fill (`h`/`v`), and T-junctions (`cross`/`t_top`/`t_bot`/`t_left`/`t_right`).
 */
typedef struct TableBorderCharacter {
    const char *tl, *tr, *bl, *br;
    const char *h, *v;
    const char *cross, *t_top, *t_bot, *t_left, *t_right;
} TableBorderCharacter;

/** Look-up table indexed by `SC_BORDER_*` style; defined in `table_print.c`. */
extern const TableBorderCharacter border_char_sets[];


/* ── Render-time context types ───────────────────────────────────────────── */

/** Indicates which section a row belongs to during rendering. */
typedef enum {
    ROW_SECTION_DATA,
    ROW_SECTION_HEADER,
    ROW_SECTION_FOOTER,
} RowSection;

/** Tracks the rendering state of an active rowspan across multiple rows. */
typedef struct RowSpan {
    /** Originating cell that started this rowspan; `NULL` when unused. */
    const ScCell *cell;

    /** Total visual line height across all spanned rows (incl. separators). */
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

    /** Per-column flag set during inner-separator rendering. */
    bool *has_rowspan;

    /** Per-column rowspan context; entry is active when `cell != NULL`. */
    RowSpan *row_span;

    /** Total inner width of the table (sum of column widths + separators). */
    int inner_width;

    /** Computed visual height per data row, in terminal lines. */
    int *row_heights;

    /** Cached cell padding values (constant for the whole print pass). */
    int pad_left;
    int pad_right;
    int pad_top;
    int pad_vertical;
} Table;


/* ── Helper predicates ───────────────────────────────────────────────────── */

/**
 * Returns the physical index of the visual first column (the "header column").
 * Equals `column_count - 1` for right-to-left layouts, `0` otherwise.
 */
static inline size_t table_physical_header_col_index(const Table *table) {
    return table->opts.right_to_left
        ? table->table_data->column_count - 1 : 0;
}

/**
 * Returns `true` when `col` is the visual first column and header-column
 * styling is enabled.
 */
static inline bool table_is_header_column(const Table *table, size_t col) {
    return table->opts.header.col
        && col == table_physical_header_col_index(table);
}

/**
 * Returns `true` when a vertical separator should be drawn after `col`.
 * Honors `border.no_inner_v` while always keeping the header-column edge.
 */
static inline bool table_has_vertical_separator_after(
    const Table *table, size_t col
) {
    if (table->opts.border.style == SC_BORDER_NONE) { return false; }
    return !table->opts.border.no_inner_v
        || table_is_header_column(table, col);
}


/* ── Specification structs ───────────────────────────────────────────────── */

/**
 * Visual specification for one horizontal border line.
 *
 * `inner_color` styles fill and junction characters; `edge_color` styles
 * the left and right corners. When `use_header_col_sep_color` is `true`,
 * `border.header_col_sep_color` overrides `inner_color` at the
 * header-column junction (section-internal separators only).
 */
typedef struct HBorderSpec {
    const char *left_corner_char;
    const char *right_corner_char;
    const char *fill_char;
    const char *column_separator;
    ScColor inner_color;
    ScColor edge_color;
    bool use_header_col_sep_color;
} HBorderSpec;


/* ── Cross-module function declarations ──────────────────────────────────── */

/* table_print_init.c */
void table_init(Table *table, const ScTableData *data, ScTableOpts opts);

/* table_print_render.c */
void table_render(Table *table);

/* table_print_render_cell.c */
TLine *make_cell_lines(const ScCell *cell, size_t *out_count);
TLine *wrap_cell_lines(const ScCell *cell, int max_width, size_t *out_count);
size_t cell_visible_width(const ScCell *cell);
size_t count_cell_lines(
    const ScCell *cell, const ScColOpts *col_opts, int available_width
);
void free_tlines(TLine *lines, size_t count);

/* table_print_render_border.c */
void render_horizontal_border(
    const Table *table, HBorderSpec spec, const bool *rowspan_flags
);
void render_inner_separator(Table *table);
void render_title_line(const Table *table, bool is_top);
void print_left_margin(const Table *table);
void print_colored_string(const char *str, ScColor color);
void print_spaces_with_bg(int count, ScColor bg);
void print_span_with_bg(
    const char *text, ScTextStyle style, ScColor cell_bg
);
void print_tline_in_width(
    const TLine *line, int width, ScHAlign halign, ScColor bg
);

/* table_print_render_row.c */
void render_row(
    Table *table,
    const ScCell *cells,
    ScColor row_bg,
    RowSection section,
    int explicit_row_height
);
