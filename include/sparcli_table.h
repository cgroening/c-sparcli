#pragma once

#include "sparcli_core.h"    // IWYU pragma: export
#include "sparcli_text.h"    // IWYU pragma: export
#include "sparcli_markup.h"  // IWYU pragma: export
#include <stddef.h>

SPARCLI_BEGIN_DECLS

/**
 * Discriminator tag for `ScCell.kind`:
 * determines which content field is active.
 */
typedef enum {
    /**< Plain C-string cell; content is in `ScCell.str`. */
    SC_CELL_STR,

    /**< Rich-text cell; content is in `ScCell.text` (not owned). */
    SC_CELL_TEXT,

    /**< Parsed-markup cell; content is in `ScCell.text` (owned by table). */
    SC_CELL_MARKUP,
} ScCellKind;

/** One cell in a table row. */
typedef struct ScCell {
    /** Content type; determines which of `str` or `text` is active. */
    ScCellKind kind;

    /** Plain string content; not owned; active when `kind == SC_CELL_STR`. */
    const char *str;

    /**
     * Rich-text content; `SC_CELL_TEXT`: not owned; `SC_CELL_MARKUP`:
     * owned by table.
     */
    ScText *text;

    /** When `true`, `align` overrides the column's horizontal alignment. */
    bool align_set;

    /**
     * Horizontal alignment override; effective only when `align_set` is `true`.
     */
    ScHAlign align;

    /** When `true`, `valign` overrides the column's vertical alignment. */
    bool valign_set;

    /**
     * Vertical alignment override; effective only when `valign_set` is `true`.
     */
    ScVAlign valign;

    /**
     * Colspan: `0`/`1` = normal; `>1` = spans N columns; `-1` = skip (covered
     * by a colspan).
     */
    int col_span;

    /**
     * Rowspan: `0`/`1` = normal; `>1` = spans N rows; `-1` = skip (covered
     * by a rowspan).
     */
    int row_span;
} ScCell;

/** Creates a plain-string cell. */
static inline ScCell sc_cell(const char *s) {
    return (ScCell){ .kind = SC_CELL_STR, .str = s };
}

/**
 * Creates a plain-string cell with explicit horizontal and vertical alignment.
 */
static inline ScCell sc_cell_a(const char *s, ScHAlign ha, ScVAlign va) {
    return (ScCell){ .kind = SC_CELL_STR, .str = s,
                     .align_set = true, .align = ha,
                     .valign_set = true, .valign = va };
}

/** Creates a rich-text cell (not owned). */
static inline ScCell sc_cell_t(ScText *t) {
    return (ScCell){ .kind = SC_CELL_TEXT, .text = t };
}

/**
 * Creates a rich-text cell with explicit horizontal and vertical alignment
 * (not owned).
 */
static inline ScCell sc_cell_ta(ScText *t, ScHAlign ha, ScVAlign va) {
    return (ScCell){ .kind = SC_CELL_TEXT, .text = t,
                     .align_set = true, .align = ha,
                     .valign_set = true, .valign = va };
}

/** Creates a plain-string cell spanning `cs` columns. */
static inline ScCell sc_cell_cs(const char *s, int cs) {
    return (ScCell){ .kind = SC_CELL_STR, .str = s, .col_span = cs };
}

/** Creates a plain-string colspan cell with explicit horizontal alignment. */
static inline ScCell sc_cell_csa(const char *s, int cs, ScHAlign ha) {
    return (ScCell){ .kind = SC_CELL_STR, .str = s,
                     .align_set = true, .align = ha, .col_span = cs };
}

/** Creates a rich-text cell spanning `cs` columns (not owned). */
static inline ScCell sc_cell_tcs(ScText *t, int cs) {
    return (ScCell){ .kind = SC_CELL_TEXT, .text = t, .col_span = cs };
}

/**
 * Creates a rich-text colspan cell with explicit horizontal alignment
 * (not owned).
 */
static inline ScCell sc_cell_tcsa(ScText *t, int cs, ScHAlign ha) {
    return (ScCell){ .kind = SC_CELL_TEXT, .text = t,
                     .align_set = true, .align = ha, .col_span = cs };
}

/** Creates a placeholder cell covered by a preceding colspan. */
static inline ScCell sc_cell_skip(void) {
    return (ScCell){ .kind = SC_CELL_STR, .str = "", .col_span = -1 };
}

/** Creates a plain-string cell spanning `rs` rows. */
static inline ScCell sc_cell_rs(const char *s, int rs) {
    return (ScCell){ .kind = SC_CELL_STR, .str = s, .row_span = rs };
}

/** Creates a rich-text cell spanning `rs` rows (not owned). */
static inline ScCell sc_cell_trs(ScText *t, int rs) {
    return (ScCell){ .kind = SC_CELL_TEXT, .text = t, .row_span = rs };
}

/** Creates a placeholder cell covered by a preceding rowspan. */
static inline ScCell sc_row_skip(void) {
    return (ScCell){ .kind = SC_CELL_STR, .str = "", .row_span = -1 };
}

/**
  * Parses `s` as inline markup and creates a cell that owns the
  * resulting `ScText` (freed automatically by `sc_table_free`).
  */
static inline ScCell sc_cell_m(const char *s) {
    return (ScCell){ .kind = SC_CELL_MARKUP, .text = sc_markup_parse(s) };
}

/** Per-column display options. */
typedef struct ScColOpts {
    /** Minimum column width in terminal columns; `0` = no minimum. */
    int min_width;

    /** Maximum column width in terminal columns; `0` = no maximum. */
    int max_width;

    /**
     * Fixed column width; `0` = auto; overrides `min_width`/`max_width`
     * when `> 0`.
     */
    int fixed_width;

    /** Horizontal alignment of cell content. */
    ScHAlign halign;

    /** Vertical alignment of cell content. */
    ScVAlign valign;

    /**
     * When `true`, wrap long lines on word boundaries;
     * when `false`, truncate.
     */
    bool word_wrap;

    /** Column background color; `SC_ANSI_COLOR_NONE` = not set. */
    ScColor bg;
} ScColOpts;

/** Border style and color settings for a table. */
typedef struct ScTableBorder {
    /** Border style (none, ascii, single, double, rounded, thick). */
    ScBorderType style;

    /** Color of the outer frame. */
    ScColor outer_color;

    /** Color of inner row and column separators. */
    ScColor inner_color;

    /**
     * Color of the header/data separator line; falls back to `inner_color`
     * when not set.
     */
    ScColor header_row_sep_color;

    /**
     * Color of the header column separator; falls back to `inner_color`
     * when not set.
     */
    ScColor header_col_sep_color;

    /** When `true`, suppress the outer frame entirely. */
    bool no_outer;

    /** When `true`, suppress inner horizontal (row) separators. */
    bool no_inner_h;

    /**
    * When `true`, suppress inner vertical (column) separators (except the
    * header column).
    */
    bool no_inner_v;
} ScTableBorder;

/** Header row and column settings. */
typedef struct ScTableHeader {
    /** When `true`, treat the first added row as the header row. */
    bool row;

    /** When `true`, treat the first column as a header column. */
    bool col;

    /** Background color for the header row. */
    ScColor row_bg;

    /** Background color for the header column. */
    ScColor col_bg;

    /** Text style applied to all header cells. */
    ScTextStyle style;
} ScTableHeader;

/** Footer row and column settings. */
typedef struct ScTableFooter {
    /** Background color for footer rows. */
    ScColor row_bg;

    /** Background color for the footer column. */
    ScColor col_bg;

    /** Text style applied to all footer cells. */
    ScTextStyle style;
} ScTableFooter;

/** Rendering options passed to `sc_table_print()`. */
typedef struct ScTableOpts {
    /** Border style and color settings. */
    ScTableBorder border;

    /** Header row and column settings. */
    ScTableHeader header;

    /** When `true`, apply alternating background colors to data rows. */
    bool striped;

    /**
     * Background color for odd-indexed (0-based) data rows when `striped`
     * is `true`.
     */
    ScColor stripe_bg;

    /** Footer row and column settings. */
    ScTableFooter footer;

    /** Table title (text, style, alignment, position). */
    ScTitle title;

    /** Inner cell padding (top/right/bottom/left). */
    ScEdges cell_pad;

    /** Outer table margin (top/right/bottom/left). */
    ScEdges margin;

    /** `0` = auto-size; `>0` = distribute total width across flex columns. */
    int total_width;

    /** `0` = unlimited; `>0` = truncate data rows at this count with an indicator. */
    int max_rows;

    /** When `true`, reverse the display order of columns. */
    bool right_to_left;
} ScTableOpts;

typedef struct ScTableData ScTableData;

ScTableData *sc_table_new(void);
SPARCLI_EXPORT void sc_table_add_column(
    ScTableData *table, const char *header, ScColOpts col_opts
);
SPARCLI_EXPORT void sc_table_add_row(
    ScTableData *table, ScCell *cells, size_t cell_count
);
SPARCLI_EXPORT void sc_table_add_row_bg(
    ScTableData *table, ScCell *cells, size_t cell_count, ScColor bg
);
SPARCLI_EXPORT void sc_table_add_footer_row(
    ScTableData *table, ScCell *cells, size_t cell_count
);
SPARCLI_EXPORT void sc_table_print(const ScTableData *table, ScTableOpts opts);
SPARCLI_EXPORT void sc_table_free(ScTableData *table);

SPARCLI_END_DECLS
