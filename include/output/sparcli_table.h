#pragma once

#include "core/sparcli_core.h"    // IWYU pragma: export
#include "core/sparcli_text.h"    // IWYU pragma: export
#include "output/sparcli_markup.h"  // IWYU pragma: export
#include <stddef.h>

SPARCLI_BEGIN_DECLS

/**
 * Discriminator tag for `ScCell.kind`:
 * determines which content field is active.
 */
typedef enum ScCellKind {
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

    /** When `true`, `halign` overrides the column's horizontal alignment. */
    bool halign_set;

    /**
     * Horizontal alignment override; effective only when `halign_set` is `true`.
     */
    ScHAlign halign;

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
                     .halign_set = true, .halign = ha,
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
                     .halign_set = true, .halign = ha,
                     .valign_set = true, .valign = va };
}

/** Creates a plain-string cell spanning `cs` columns. */
static inline ScCell sc_cell_cs(const char *s, int cs) {
    return (ScCell){ .kind = SC_CELL_STR, .str = s, .col_span = cs };
}

/** Creates a plain-string colspan cell with explicit horizontal alignment. */
static inline ScCell sc_cell_csa(const char *s, int cs, ScHAlign ha) {
    return (ScCell){ .kind = SC_CELL_STR, .str = s,
                     .halign_set = true, .halign = ha, .col_span = cs };
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
                     .halign_set = true, .halign = ha, .col_span = cs };
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


/* ── Functional variants of the inline cell helpers (FFI-friendly) ──── */

/**
 * Functional equivalents of the `sc_cell*` static-inline helpers above.
 *
 * Bindings that cannot consume `static inline` functions or C99 compound
 * literals (Python via ctypes, some Rust binding setups) call these
 * instead. Native C callers should prefer the inline forms.
 */
SPARCLI_EXPORT ScCell sc_cell_from_str(const char *str);
SPARCLI_EXPORT ScCell sc_cell_str_aligned(
    const char *str, ScHAlign halign, ScVAlign valign
);
SPARCLI_EXPORT ScCell sc_cell_from_text(ScText *text);
SPARCLI_EXPORT ScCell sc_cell_text_aligned(
    ScText *text, ScHAlign halign, ScVAlign valign
);
SPARCLI_EXPORT ScCell sc_cell_colspan(const char *str, int col_span);
SPARCLI_EXPORT ScCell sc_cell_rowspan(const char *str, int row_span);
SPARCLI_EXPORT ScCell sc_cell_skip_placeholder(void);
SPARCLI_EXPORT ScCell sc_row_skip_placeholder(void);
SPARCLI_EXPORT ScCell sc_cell_from_markup(const char *markup);


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
    ScBorderType type;

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

/**
 * Allocates an empty table. Free with `sc_table_free`.
 *
 * @return  Heap-allocated table; `NULL` on allocation failure.
 */
SPARCLI_EXPORT ScTableData *sc_table_new(void);

/**
 * Appends a column with the given header and per-column options.
 *
 * @param table     Target table; no-op when `NULL`.
 * @param header    Column header string; copied internally; may be `NULL`.
 * @param col_opts  Per-column display options.
 *
 * @note `header` is `strdup`-ed; the caller may free it immediately
 * after this call returns.
 */
SPARCLI_EXPORT void sc_table_add_column(
    ScTableData *table, const char *header, ScColOpts col_opts
);

/**
 * Appends a data row. The cells are copied into the table.
 *
 * @param table       Target table; no-op when `NULL`.
 * @param cells       Cell array; one entry per column. Cells beyond
 *                    `cell_count` are filled with empty cells.
 * @param cell_count  Number of valid entries in `cells`.
 *
 * @note For `SC_CELL_STR` and `SC_CELL_TEXT` cells the table stores the
 * original pointers (`ScCell.str` is `const char *`; `ScCell.text` is
 * `ScText *`). Those strings/objects must outlive the next
 * `sc_table_print`. For `SC_CELL_MARKUP` cells the table takes
 * ownership of the parsed `ScText`.
 */
SPARCLI_EXPORT void sc_table_add_row(
    ScTableData *table, ScCell *cells, size_t cell_count
);

/** Same as `sc_table_add_row`, but sets an explicit row background. */
SPARCLI_EXPORT void sc_table_add_row_bg(
    ScTableData *table, ScCell *cells, size_t cell_count, ScColor bg
);

/** Appends a footer row; same ownership rules as `sc_table_add_row`. */
SPARCLI_EXPORT void sc_table_add_footer_row(
    ScTableData *table, ScCell *cells, size_t cell_count
);

/**
 * Renders `table` to the current output stream.
 *
 * @param table  Borrowed table; not retained or modified.
 * @param opts   Rendering options (passed by value).
 */
SPARCLI_EXPORT void sc_table_print(const ScTableData *table, ScTableOpts opts);

/**
 * Frees `table` and any owned cells (including the `ScText` of
 * `SC_CELL_MARKUP` cells). Safe to call with `NULL`.
 */
SPARCLI_EXPORT void sc_table_free(ScTableData *table);

SPARCLI_END_DECLS
