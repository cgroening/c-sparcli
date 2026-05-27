#pragma once

#include "sparcli.h"                    // IWYU pragma: export
#include "internal.h"                   // IWYU pragma: export
#include "table_internal.h"             // IWYU pragma: export

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "table_print_render_cell.c"    // IWYU pragma: export
#include "table_print_render_border.c"  // IWYU pragma: export
#include "table_print_render_row.c"     // IWYU pragma: export

static void table_render(Table *table);
    static void render_top_border(const Table *table);
    static void render_header_row(const Table *table);
        static void render_section_sep(const Table *table);
    static void render_data_rows(Table *table);
        static size_t compute_max_rows(const Table *table);
        static void init_rowspan_contexts(Table *table, size_t r);
            static int compute_rowspan_height(
                const Table *table, size_t r, int rs, int sep_h
            );
        static void render_data_row_sep(Table *table, size_t r);
        static ScColor resolve_data_row_bg(const Table *table, size_t r);
        static void advance_rowspan_vis_offsets(Table *table, int row_h);
        static void render_truncation_indicator(Table *table, size_t max_r);
    static void render_footer_rows(const Table *table);
    static void render_bottom_border(const Table *table);


/**
 * Orchestrates the rendering of the entire table.
 *
 * Top-level render sequence:
 * margins → border → header → data → footer → margins.
 */
static void table_render(Table *table) {
    print_empty_lines(table->opts.margin.top);
    render_top_border(table);
    render_header_row(table);
    render_data_rows(table);
    render_footer_rows(table);
    render_bottom_border(table);
    print_empty_lines(table->opts.margin.bottom);
}

/**
 * Renders the top border: Title line if a top title is set, otherwise a plain
 * horizontal border. If `no_outer` is set, the border is omitted entirely
 * (including the title line).
 */
static void render_top_border(const Table *table) {
    ScBorderType border_style = table->opts.border.style;

    if (table->opts.border.no_outer) { return; }

    if (table->opts.title.text && table->opts.title.pos == SC_POSITION_TOP) {
        render_title_line(table, 1);
    } else if (border_style != SC_BORDER_NONE) {
        render_horizontal_border(table, (HBorderSpec){
            .left_corner_char  = border_char_sets[border_style].tl,
            .right_corner_char = border_char_sets[border_style].tr,
            .fill_char         = border_char_sets[border_style].h,
            .column_separator  = border_char_sets[border_style].t_top,
            .inner_color             = table->opts.border.outer_color,
            .edge_color        = table->opts.border.outer_color,
        }, NULL);
    }
}

/** Renders the column-name header row followed by a section separator. */
static void render_header_row(const Table *table) {
    const ScTableData *table_data = table->table_data;
    if (!table->opts.header.row) { return; }

    ScCell *hcells = malloc(table_data->column_count * sizeof(ScCell));
    for (size_t c = 0; c < table_data->column_count; c++) {
        hcells[c] = sc_cell(
            table_data->columns[c].header ? table_data->columns[c].header : ""
        );
    }
    render_row(table, hcells, table->opts.header.row_bg, 1, 0);
    free(hcells);

    if (table->opts.border.style != SC_BORDER_NONE) {
        render_section_sep(table);
    }
}

/**
 * Renders the section-boundary separator used after the header and before the
 * footer. Uses `header_row_sep_color` when set, otherwise `inner_color`.
 */
static void render_section_sep(const Table *table) {
    ScBorderType border_style = table->opts.border.style;

    ScColor inner_color;
    if (table->opts.border.header_row_sep_color.index != -2) {
        inner_color = table->opts.border.header_row_sep_color;;
    } else {
        inner_color = table->opts.border.inner_color;
    }

    render_horizontal_border(table, (HBorderSpec){
        .left_corner_char   = border_char_sets[border_style].t_left,
        .right_corner_char  = border_char_sets[border_style].t_right,
        .fill_char          = border_char_sets[border_style].h,
        .column_separator   = border_char_sets[border_style].cross,
        .inner_color        = inner_color,
        .edge_color         = table->opts.border.outer_color,
        .use_header_col_sep = true,
    }, NULL);
}

/**
 * Renders all data rows with rowspan tracking, inner separators and the
 *  optional truncation indicator when `max_rows` is exceeded.
 */
static void render_data_rows(Table *table) {
    const ScTableData *table_data = table->table_data;
    size_t max_rows = compute_max_rows(table);

    for (size_t r = 0; r < max_rows; r++) {
        init_rowspan_contexts(table, r);
        if (r > 0) { render_data_row_sep(table, r); }
        ScColor row_bg = resolve_data_row_bg(table, r);
        render_row(
            table, table_data->rows[r].cells, row_bg, 0, table->row_heights[r]
        );
        advance_rowspan_vis_offsets(table, table->row_heights[r]);
    }

    if (table->opts.max_rows > 0 && table_data->row_count > max_rows) {
        render_truncation_indicator(table, max_rows);
    }
}

/** Returns the effective row count, clamped to `opts.max_rows` when set. */
static size_t compute_max_rows(const Table *table) {
    size_t max_rows = table->table_data->row_count;

    if (table->opts.max_rows > 0 && (size_t)table->opts.max_rows < max_rows) {
        max_rows = (size_t)table->opts.max_rows;
    }

    return max_rows;
}

/**
 * Initializes `row_span[]` for the row at the given index:
 * Creates a `RowSpan` for rowspan-starting cells, clears it for normal cells
 * and leaves continuation entries unchanged.
 */
static void init_rowspan_contexts(Table *table, size_t row_index) {
    const ScTableData *table_data = table->table_data;
    const ScTableBorder border = table->opts.border;

    // Determine the height of inner separators, which affects the total height
    // of rowspan cells and thus the vertical alignment of their content.
    int inner_separator_height;
    if (border.style != SC_BORDER_NONE && !border.no_inner_h) {
        inner_separator_height = 1;
    } else {
        inner_separator_height = 0;
    }

    // Initialize rowspan contexts for this row, starting new ones where
    // rowspan starts
    for (size_t i = 0; i < table_data->column_count; i++) {
        int row_span = table_data->rows[row_index].cells[i].row_span;
        if (row_span > 1) {
            ScVAlign vertical_align = table_data->columns[i].opts.valign;
            if (table_data->rows[row_index].cells[i].valign_set) {
                vertical_align = table_data->rows[row_index].cells[i].valign;
            }
            int row_span_height = compute_rowspan_height(
                table, row_index, row_span, inner_separator_height
            );
            table->row_span[i] = (RowSpan){
                &table_data->rows[row_index].cells[i],
                row_span_height,
                0,
                vertical_align
            };
        } else if (row_span != -1) {
            table->row_span[i] = (RowSpan){ NULL, 0, 0, 0 };
        }
    }
}

/**
 * Sums the visual heights `row_count` rows starting at the `row_index`,
 * adding @p separator_height each internal row boundary.
 * Returns the total span height.
 */
static int compute_rowspan_height(
    const Table *table, size_t row_index, int row_count, int separator_height
) {
    const ScTableData *table_data = table->table_data;
    int sh = 0;
    for (
        int k = 0;
        k < row_count && row_index + (size_t)k < table_data->row_count;
        k++
    ) {
        if (k > 0) { sh += separator_height; }
        sh += table->row_heights[row_index + k];
    }
    return sh;
}

/**
 * Marks active rowspan columns in `has_rowspan[]`, renders the inner separator
 * line, then advances line_offset by 1 for each spanning cell that crossed it.
 */
static void render_data_row_sep(Table *table, size_t row_index) {
    const ScTableData *table_data = table->table_data;
    if (
        table->opts.border.style == SC_BORDER_NONE ||
        table->opts.border.no_inner_h
    ) {
        return;
    }

    for (size_t i = 0; i < table_data->column_count; i++) {
        table->has_rowspan[i] = (
            table_data->rows[row_index].cells[i].row_span == -1
        );
    }
    render_inner_sep(table);
    for (size_t i = 0; i < table_data->column_count; i++) {
        if (table->has_rowspan[i] && table->row_span[i].cell) {
            table->row_span[i].line_offset++;
        }
    }
}

/**
 * Returns the background color for data row `row_index`: explicit row bg if set,
 * stripe bg for odd rows when striping is on, otherwise `SC_ANSI_COLOR_NONE`.
 */
static ScColor resolve_data_row_bg(const Table *table, size_t row_index) {
    ScColor row_bg = table->table_data->rows[row_index].bg;
    if (row_bg.index == -2) {
        row_bg = SC_ANSI_COLOR_NONE;
        if (table->opts.striped && (row_index % 2 == 1)) {
            row_bg = table->opts.stripe_bg;
        }
    }
    return row_bg;
}

/**
 * Advances line_offset by `row_height` for all active rowspan cells after a row
 * has been rendered.
 */
static void advance_rowspan_vis_offsets(Table *table, int row_height) {
    const ScTableData *table_data = table->table_data;
    for (size_t i = 0; i < table_data->column_count; i++) {
        if (table->row_span[i].cell) {
            table->row_span[i].line_offset += row_height;
        }
    }
}

/**
 * Renders a single dimmed full-width row showing how many rows were omitted
 *  due to `opts.max_rows`.
 */
static void render_truncation_indicator(Table *table, size_t max_rows) {
    const ScTableData *table_data = table->table_data;
    char truncation_message[64];
    snprintf(
        truncation_message,
        sizeof(truncation_message),
        "… %zu more rows",
        table_data->row_count - max_rows
    );

    ScTextStyle dim_style = {
        SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
    };
    ScCell *indicator_cells = malloc(table_data->column_count * sizeof(ScCell));
    indicator_cells[0] = sc_cell_cs(
        truncation_message, (int)table_data->column_count
    );

    for (size_t i = 1; i < table_data->column_count; i++) {
        indicator_cells[i] = sc_cell_skip();
    }

    ScTextStyle saved = table->opts.header.opts;
    table->opts.header.opts = dim_style;
    render_row(table, indicator_cells, SC_ANSI_COLOR_NONE, 1, 0);
    table->opts.header.opts = saved;
    free(indicator_cells);
}

/** Renders footer rows, preceded by a section separator. */
static void render_footer_rows(const Table *table) {
    const ScTableData *table_data = table->table_data;
    ScBorderType border_style = table->opts.border.style;

    if (table_data->footer_row_count > 0 && border_style != SC_BORDER_NONE) {
        render_section_sep(table);
    }

    for (size_t i = 0; i < table_data->footer_row_count; i++) {
        if (
            i > 0 &&
            border_style != SC_BORDER_NONE &&
            !table->opts.border.no_inner_h
        ) {
            render_horizontal_border(table, (HBorderSpec){
                .left_corner_char   = border_char_sets[border_style].t_left,
                .right_corner_char  = border_char_sets[border_style].t_right,
                .fill_char          = border_char_sets[border_style].h,
                .column_separator   = border_char_sets[border_style].cross,
                .inner_color        = table->opts.border.inner_color,
                .edge_color         = table->opts.border.outer_color,
                .use_header_col_sep = true,
            }, NULL);
        }
        render_row(
            table,
            table_data->footer_rows[i].cells,
            table->opts.footer.row_bg,
            2,
            0
        );
    }
}

/**
 * Renders the bottom border: title line if a bottom title is set, otherwise a
 * plain horizontal border. No-op when no_outer is set.
 */
static void render_bottom_border(const Table *table) {
    ScBorderType border_style = table->opts.border.style;
    if (table->opts.border.no_outer) {
        return;
    }

    if (table->opts.title.text && table->opts.title.pos == SC_POSITION_BOTTOM) {
        render_title_line(table, 0);
    } else if (border_style != SC_BORDER_NONE) {
        render_horizontal_border(table, (HBorderSpec){
            .left_corner_char  = border_char_sets[border_style].bl,
            .right_corner_char = border_char_sets[border_style].br,
            .fill_char         = border_char_sets[border_style].h,
            .column_separator  = border_char_sets[border_style].t_bot,
            .inner_color       = table->opts.border.outer_color,
            .edge_color        = table->opts.border.outer_color,
        }, NULL);
    }
}
