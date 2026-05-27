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
            static int compute_rowspan_height(const Table *table, size_t r, int rs, int sep_h);
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
            .color             = table->opts.border.outer_color,
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

/** Renders the section-boundary separator used after the header and before the
 *  footer. Uses header_row_sep_color when set, otherwise inner_color. */
static void render_section_sep(const Table *table) {
    ScBorderType bs = table->opts.border.style;
    ScColor oc      = table->opts.border.outer_color;
    ScColor ic      = table->opts.border.inner_color;
    ScColor hrsc    = table->opts.border.header_row_sep_color;
    ScColor hc      = (hrsc.index != -2) ? hrsc : ic;
    render_horizontal_border(table, (HBorderSpec){
        .left_corner_char   = border_char_sets[bs].t_left,
        .right_corner_char  = border_char_sets[bs].t_right,
        .fill_char          = border_char_sets[bs].h,
        .column_separator   = border_char_sets[bs].cross,
        .color              = hc,
        .edge_color         = oc,
        .use_header_col_sep = true,
    }, NULL);
}

/** Renders all data rows with rowspan tracking, inner separators, and the
 *  optional truncation indicator when max_rows is exceeded. */
static void render_data_rows(Table *table) {
    const ScTableData *table_data = table->table_data;
    size_t max_r = compute_max_rows(table);

    for (size_t r = 0; r < max_r; r++) {
        init_rowspan_contexts(table, r);
        if (r > 0) { render_data_row_sep(table, r); }
        ScColor row_bg = resolve_data_row_bg(table, r);
        render_row(table, table_data->rows[r].cells, row_bg, 0, table->row_heights[r]);
        advance_rowspan_vis_offsets(table, table->row_heights[r]);
    }

    if (table->opts.max_rows > 0 && table_data->row_count > max_r) {
        render_truncation_indicator(table, max_r);
    }
}

/** Returns the effective row count, clamped to opts.max_rows when set. */
static size_t compute_max_rows(const Table *table) {
    size_t max_r = table->table_data->row_count;
    if (table->opts.max_rows > 0 && (size_t)table->opts.max_rows < max_r) {
        max_r = (size_t)table->opts.max_rows;
    }
    return max_r;
}

/** Initialises rsc[] for row @p r: creates an RSCtx for rowspan-starting cells,
 *  clears it for normal cells, and leaves rowspan-continuation entries unchanged. */
static void init_rowspan_contexts(Table *table, size_t r) {
    const ScTableData *table_data = table->table_data;
    ScBorderType bs = table->opts.border.style;
    int sep_h = (bs != SC_BORDER_NONE && !table->opts.border.no_inner_h) ? 1 : 0;

    for (size_t c = 0; c < table_data->column_count; c++) {
        int rs = table_data->rows[r].cells[c].rowspan;
        if (rs > 1) {
            int sh = compute_rowspan_height(table, r, rs, sep_h);
            ScVAlign va = table_data->columns[c].opts.valign;
            if (table_data->rows[r].cells[c].valign_set) { va = table_data->rows[r].cells[c].valign; }
            table->rsc[c] = (RSCtx){ &table_data->rows[r].cells[c], sh, 0, va };
        } else if (rs != -1) {
            table->rsc[c] = (RSCtx){ NULL, 0, 0, 0 };
        }
    }
}

/** Sums the visual heights of @p rs rows starting at @p r, adding @p sep_h for
 *  each internal row boundary. Returns the total span height. */
static int compute_rowspan_height(const Table *table, size_t r, int rs, int sep_h) {
    const ScTableData *table_data = table->table_data;
    int sh = 0;
    for (int k = 0; k < rs && r + (size_t)k < table_data->row_count; k++) {
        if (k > 0) { sh += sep_h; }
        sh += table->row_heights[r + k];
    }
    return sh;
}

/** Marks active rowspan columns in is_rs[], renders the inner separator line,
 *  then advances vis_offset by 1 for each spanning cell that crossed it. */
static void render_data_row_sep(Table *table, size_t r) {
    const ScTableData *table_data = table->table_data;
    ScBorderType bs = table->opts.border.style;
    if (bs == SC_BORDER_NONE || table->opts.border.no_inner_h) { return; }

    for (size_t c = 0; c < table_data->column_count; c++) {
        table->is_rs[c] = (table_data->rows[r].cells[c].rowspan == -1);
    }
    render_inner_sep(table);
    for (size_t c = 0; c < table_data->column_count; c++) {
        if (table->is_rs[c] && table->rsc[c].cell) { table->rsc[c].vis_offset++; }
    }
}

/** Returns the background color for data row @p r: explicit row bg if set,
 *  stripe bg for odd rows when striping is on, otherwise SC_ANSI_COLOR_NONE. */
static ScColor resolve_data_row_bg(const Table *table, size_t r) {
    ScColor row_bg = table->table_data->rows[r].bg;
    if (row_bg.index == -2) {
        row_bg = SC_ANSI_COLOR_NONE;
        if (table->opts.striped && (r % 2 == 1)) { row_bg = table->opts.stripe_bg; }
    }
    return row_bg;
}

/** Advances vis_offset by @p row_h for all active rowspan cells after a row
 *  has been rendered. */
static void advance_rowspan_vis_offsets(Table *table, int row_h) {
    const ScTableData *table_data = table->table_data;
    for (size_t c = 0; c < table_data->column_count; c++) {
        if (table->rsc[c].cell) { table->rsc[c].vis_offset += row_h; }
    }
}

/** Renders a single dimmed full-width row showing how many rows were omitted
 *  due to opts.max_rows. */
static void render_truncation_indicator(Table *table, size_t max_r) {
    const ScTableData *table_data = table->table_data;
    char msg[64];
    snprintf(msg, sizeof(msg), "… %zu more rows", table_data->row_count - max_r);
    ScTextStyle dim = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    ScCell *ind = malloc(table_data->column_count * sizeof(ScCell));
    ind[0] = sc_cell_cs(msg, (int)table_data->column_count);
    for (size_t c = 1; c < table_data->column_count; c++) { ind[c] = sc_cell_skip(); }
    ScTextStyle saved = table->opts.header.opts;
    table->opts.header.opts = dim;
    render_row(table, ind, SC_ANSI_COLOR_NONE, 1, 0);
    table->opts.header.opts = saved;
    free(ind);
}

/** Renders footer rows, preceded by a section separator. */
static void render_footer_rows(const Table *table) {
    const ScTableData *table_data = table->table_data;
    ScBorderType bs = table->opts.border.style;
    ScColor oc      = table->opts.border.outer_color;
    ScColor ic      = table->opts.border.inner_color;

    if (table_data->footer_row_count > 0 && bs != SC_BORDER_NONE) {
        render_section_sep(table);
    }

    for (size_t r = 0; r < table_data->footer_row_count; r++) {
        if (r > 0 && bs != SC_BORDER_NONE && !table->opts.border.no_inner_h) {
            render_horizontal_border(table, (HBorderSpec){
                .left_corner_char = border_char_sets[bs].t_left, .right_corner_char = border_char_sets[bs].t_right,
                .fill_char = border_char_sets[bs].h,    .column_separator = border_char_sets[bs].cross,
                .color = ic,               .edge_color = oc,
                .use_header_col_sep = true,
            }, NULL);
        }
        render_row(table, table_data->footer_rows[r].cells, table->opts.footer.row_bg, 2, 0);
    }
}

/* Renders the bottom border: title line if a bottom title is set, otherwise a
   plain horizontal border. No-op when no_outer is set. */
static void render_bottom_border(const Table *table) {
    ScBorderType bs = table->opts.border.style;
    ScColor oc = table->opts.border.outer_color;
    if (table->opts.border.no_outer) { return; }
    if (table->opts.title.text && table->opts.title.pos == SC_POSITION_BOTTOM) {
        render_title_line(table, 0);
    } else if (bs != SC_BORDER_NONE) {
        render_horizontal_border(table, (HBorderSpec){
            .left_corner_char = border_char_sets[bs].bl,    .right_corner_char = border_char_sets[bs].br,
            .fill_char = border_char_sets[bs].h,   .column_separator = border_char_sets[bs].t_bot,
            .color = oc,         .edge_color = oc,
        }, NULL);
    }
}
