#include "sparcli.h"
#include "internal.h"
#include "table/table_internal.h"

#include <stdio.h>
#include <stdlib.h>


/** Length of the truncation indicator string buffer. */
#define TRUNCATION_MESSAGE_BUFFER 64


// Forward declarations indented to reflect call hierarchy

static void render_top_border(const Table *table);
static void render_header_row(const Table *table);
    static void render_section_separator(const Table *table);

static void render_data_rows(Table *table);
    static size_t effective_row_count(const Table *table);
    static void init_rowspan_contexts(Table *table, size_t row_index);
        static int compute_rowspan_total_height(
            const Table *table, size_t row_index,
            int row_span, int separator_height
        );
    static void render_data_row_separator(Table *table, size_t row_index);
    static ScColor resolve_data_row_bg(const Table *table, size_t row_index);
    static void advance_rowspan_offsets(Table *table, int row_height);
    static void render_truncation_indicator(Table *table, size_t max_rows);

static void render_footer_rows(const Table *table);
static void render_bottom_border(const Table *table);


void table_render(Table *table) {
    sc_print_newlines(table->opts.margin.top);
    render_top_border(table);
    render_header_row(table);
    render_data_rows(table);
    render_footer_rows(table);
    render_bottom_border(table);
    sc_print_newlines(table->opts.margin.bottom);
}




/* ── Top / bottom border ────────────────────────────────────────────────── */

/**
 * Renders the top border: title line when a top title is configured,
 * otherwise a plain horizontal border. Suppressed entirely when
 * `border.no_outer` is set.
 */
static void render_top_border(const Table *table) {
    ScBorderType style = table->opts.border.type;
    if (table->opts.border.no_outer) { return; }

    bool has_top_title = table->opts.title.text
        && table->opts.title.pos == SC_POSITION_TOP;
    if (has_top_title) {
        render_title_line(table, true);
        return;
    }
    if (style == SC_BORDER_NONE) { return; }

    render_horizontal_border(table, (HBorderSpec){
        .left_corner_char = border_char_sets[style].tl,
        .right_corner_char = border_char_sets[style].tr,
        .fill_char = border_char_sets[style].h,
        .column_separator = border_char_sets[style].t_top,
        .inner_color = table->opts.border.outer_color,
        .edge_color = table->opts.border.outer_color,
    }, NULL);
}

/**
 * Renders the bottom border: title line when a bottom title is configured,
 * otherwise a plain horizontal border. Suppressed when `border.no_outer`.
 */
static void render_bottom_border(const Table *table) {
    ScBorderType style = table->opts.border.type;
    if (table->opts.border.no_outer) { return; }

    bool has_bottom_title = table->opts.title.text
        && table->opts.title.pos == SC_POSITION_BOTTOM;
    if (has_bottom_title) {
        render_title_line(table, false);
        return;
    }
    if (style == SC_BORDER_NONE) { return; }

    render_horizontal_border(table, (HBorderSpec){
        .left_corner_char = border_char_sets[style].bl,
        .right_corner_char = border_char_sets[style].br,
        .fill_char = border_char_sets[style].h,
        .column_separator = border_char_sets[style].t_bot,
        .inner_color = table->opts.border.outer_color,
        .edge_color = table->opts.border.outer_color,
    }, NULL);
}


/* ── Header row + section separator ─────────────────────────────────────── */

/** Renders the column-name header row followed by a section separator. */
static void render_header_row(const Table *table) {
    const ScTableData *data = table->table_data;
    if (!table->opts.header.row) { return; }

    ScCell *header_cells = malloc(data->column_count * sizeof(ScCell));
    for (size_t col = 0; col < data->column_count; col++) {
        header_cells[col] = sc_cell(
            data->columns[col].header ? data->columns[col].header : ""
        );
    }
    render_row(
        (Table *)table, header_cells, table->opts.header.row_bg,
        ROW_SECTION_HEADER, 0
    );
    free(header_cells);

    if (table->opts.border.type != SC_BORDER_NONE) {
        render_section_separator(table);
    }
}

/**
 * Renders the section-boundary separator used after the header and before
 * the footer. Uses `header_row_sep_color` when set, otherwise `inner_color`.
 */
static void render_section_separator(const Table *table) {
    ScBorderType style = table->opts.border.type;

    ScColor separator_color = table->opts.border.inner_color;
    if (table->opts.border.header_row_sep_color.index != 0) {
        separator_color = table->opts.border.header_row_sep_color;
    }

    render_horizontal_border(table, (HBorderSpec){
        .left_corner_char = border_char_sets[style].t_left,
        .right_corner_char = border_char_sets[style].t_right,
        .fill_char = border_char_sets[style].h,
        .column_separator = border_char_sets[style].cross,
        .inner_color = separator_color,
        .edge_color = table->opts.border.outer_color,
        .use_header_col_sep_color = true,
    }, NULL);
}


/* ── Data rows ──────────────────────────────────────────────────────────── */

/**
 * Renders all data rows with rowspan tracking, inner separators, and the
 * optional truncation indicator when `max_rows` is exceeded.
 */
static void render_data_rows(Table *table) {
    const ScTableData *data = table->table_data;
    size_t row_count = effective_row_count(table);

    for (size_t row_index = 0; row_index < row_count; row_index++) {
        init_rowspan_contexts(table, row_index);
        if (row_index > 0) {
            render_data_row_separator(table, row_index);
        }
        ScColor row_bg = resolve_data_row_bg(table, row_index);
        render_row(
            table, data->rows[row_index].cells, row_bg,
            ROW_SECTION_DATA, table->row_heights[row_index]
        );
        advance_rowspan_offsets(table, table->row_heights[row_index]);
    }

    if (table->opts.max_rows > 0 && data->row_count > row_count) {
        render_truncation_indicator(table, row_count);
    }
}

/** Returns the effective row count, clamped to `opts.max_rows` when set. */
static size_t effective_row_count(const Table *table) {
    size_t count = table->table_data->row_count;
    if (table->opts.max_rows > 0
        && (size_t)table->opts.max_rows < count) {
        count = (size_t)table->opts.max_rows;
    }
    return count;
}

/**
 * Initializes `row_span[]` for `row_index`: creates a `RowSpan` entry for
 * rowspan-starting cells, clears it for normal cells, and leaves
 * continuation entries untouched.
 */
static void init_rowspan_contexts(Table *table, size_t row_index) {
    const ScTableData *data = table->table_data;
    const ScTableBorder border = table->opts.border;

    int separator_height = 0;
    if (border.type != SC_BORDER_NONE && !border.no_inner_h) {
        separator_height = 1;
    }

    for (size_t col = 0; col < data->column_count; col++) {
        const ScCell *cell = &data->rows[row_index].cells[col];
        int row_span = cell->row_span;

        if (row_span > 1) {
            ScVAlign valign = data->columns[col].opts.valign;
            if (cell->valign_set) { valign = cell->valign; }
            int total_height = compute_rowspan_total_height(
                table, row_index, row_span, separator_height
            );
            table->row_span[col] = (RowSpan){
                .cell = cell,
                .row_count = total_height,
                .line_offset = 0,
                .valign = valign,
            };
        } else if (row_span != -1) {
            table->row_span[col] = (RowSpan){
                .cell = NULL,
                .row_count = 0,
                .line_offset = 0,
                .valign = 0,
            };
        }
    }
}

/**
 * Sums the visual heights of `row_span` rows starting at `row_index`,
 * adding `separator_height` for each internal row boundary.
 */
static int compute_rowspan_total_height(
    const Table *table, size_t row_index,
    int row_span, int separator_height
) {
    const ScTableData *data = table->table_data;
    int total = 0;
    for (int k = 0;
         k < row_span && row_index + (size_t)k < data->row_count;
         k++) {
        if (k > 0) { total += separator_height; }
        total += table->row_heights[row_index + k];
    }
    return total;
}

/**
 * Marks active rowspan columns in `has_rowspan[]`, renders the inner
 * separator, then advances `line_offset` by 1 for each spanning cell that
 * crossed the separator.
 */
static void render_data_row_separator(Table *table, size_t row_index) {
    const ScTableData *data = table->table_data;
    if (table->opts.border.type == SC_BORDER_NONE
        || table->opts.border.no_inner_h) {
        return;
    }

    for (size_t col = 0; col < data->column_count; col++) {
        table->has_rowspan[col] =
            (data->rows[row_index].cells[col].row_span == -1);
    }
    render_inner_separator(table);
    for (size_t col = 0; col < data->column_count; col++) {
        if (table->has_rowspan[col] && table->row_span[col].cell) {
            table->row_span[col].line_offset++;
        }
    }
}

/**
 * Returns the background color for `row_index`: explicit row bg if set,
 * stripe bg for odd rows when striping is on, otherwise no color.
 */
static ScColor resolve_data_row_bg(const Table *table, size_t row_index) {
    ScColor row_bg = table->table_data->rows[row_index].bg;
    if (sc_color_is_active(row_bg)) { return row_bg; }

    if (table->opts.striped && (row_index % 2 == 1)) {
        return table->opts.stripe_bg;
    }
    return SC_ANSI_COLOR_NONE;
}

/**
 * Advances `line_offset` by `row_height` for every active rowspan after a
 * row has been rendered.
 */
static void advance_rowspan_offsets(Table *table, int row_height) {
    for (size_t col = 0; col < table->table_data->column_count; col++) {
        if (table->row_span[col].cell) {
            table->row_span[col].line_offset += row_height;
        }
    }
}

/**
 * Renders a single dimmed full-width row showing how many rows were
 * omitted because of `opts.max_rows`.
 */
static void render_truncation_indicator(Table *table, size_t max_rows) {
    const ScTableData *data = table->table_data;

    char message[TRUNCATION_MESSAGE_BUFFER];
    snprintf(
        message, sizeof(message),
        "… %zu more rows", data->row_count - max_rows
    );

    ScTextStyle dim_style = {
        .attr = SC_TEXT_ATTR_DIM,
        .fg = SC_ANSI_COLOR_NONE,
        .bg = SC_ANSI_COLOR_NONE,
    };

    ScCell *cells = malloc(data->column_count * sizeof(ScCell));
    cells[0] = sc_cell_cs(message, (int)data->column_count);
    for (size_t col = 1; col < data->column_count; col++) {
        cells[col] = sc_cell_skip();
    }

    ScTextStyle saved_header_style = table->opts.header.style;
    table->opts.header.style = dim_style;
    render_row(table, cells, SC_ANSI_COLOR_NONE, ROW_SECTION_HEADER, 0);
    table->opts.header.style = saved_header_style;
    free(cells);
}


/* ── Footer rows ────────────────────────────────────────────────────────── */

/** Renders footer rows, preceded by a section separator. */
static void render_footer_rows(const Table *table) {
    const ScTableData *data = table->table_data;
    ScBorderType style = table->opts.border.type;

    if (data->footer_row_count > 0 && style != SC_BORDER_NONE) {
        render_section_separator(table);
    }

    for (size_t i = 0; i < data->footer_row_count; i++) {
        bool needs_inner_sep = i > 0
            && style != SC_BORDER_NONE
            && !table->opts.border.no_inner_h;
        if (needs_inner_sep) {
            render_horizontal_border(table, (HBorderSpec){
                .left_corner_char = border_char_sets[style].t_left,
                .right_corner_char = border_char_sets[style].t_right,
                .fill_char = border_char_sets[style].h,
                .column_separator = border_char_sets[style].cross,
                .inner_color = table->opts.border.inner_color,
                .edge_color = table->opts.border.outer_color,
                .use_header_col_sep_color = true,
            }, NULL);
        }
        render_row(
            (Table *)table, data->footer_rows[i].cells,
            table->opts.footer.row_bg, ROW_SECTION_FOOTER, 0
        );
    }
}
