#pragma once

#include "sparcli.h"         // IWYU pragma: export
#include "internal.h"        // IWYU pragma: export
#include "table_internal.h"  // IWYU pragma: export
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "table_print_render.c"


/* ── Column width computation ────────────────────────────────────────────── */

static void compute_col_widths(Table *table) {
    const ScTableData *table_data = table->table_data;
    for (size_t c = 0; c < table_data->column_count; c++) {

        /* --- natural width: start from the header string width --- */
        const char *hdr = table_data->columns[c].header;
        size_t max_w = hdr ? sc_utf8_string_length(hdr, strlen(hdr)) : 0;

        /* --- scan data rows: widen to the widest non-spanning cell --- */
        for (size_t r = 0; r < table_data->row_count; r++) {
            if (c >= table_data->rows[r].ncols) continue;
            ScCell *cell = &table_data->rows[r].cells[c];
            if (cell->colspan != 0 && cell->colspan != 1) continue; /* skip/span */
            if (cell->rowspan == -1) continue;
            size_t w = cell_vis_width(cell);
            if (w > max_w) max_w = w;
        }

        /* --- scan footer rows: same widening pass --- */
        for (size_t r = 0; r < table_data->footer_row_count; r++) {
            if (c >= table_data->footer_rows[r].ncols) continue;
            ScCell *cell = &table_data->footer_rows[r].cells[c];
            if (cell->colspan != 0 && cell->colspan != 1) continue;
            if (cell->rowspan == -1) continue;
            size_t w = cell_vis_width(cell);
            if (w > max_w) max_w = w;
        }

        /* --- apply cell padding, then clamp to fixed/min/max constraints --- */
        int col_w = (int)max_w + table->opts.cell_pad.left + table->opts.cell_pad.right;
        const ScColOpts *co = &table_data->columns[c].opts;
        if (co->fixed_width > 0)       col_w = co->fixed_width;
        else {
            if (co->min_width > 0 && col_w < co->min_width) col_w = co->min_width;
            if (co->max_width > 0 && col_w > co->max_width) col_w = co->max_width;
        }
        table->col_widths[c] = col_w;
    }
}

/* Scale flex columns to reach total_width. */
static void apply_total_width(Table *table) {
    const ScTableData *table_data = table->table_data;
    if (table->opts.total_width <= 0) return;
    ScBorderType bs = table->opts.borders.style;
    int no_outer = table->opts.borders.no_outer;

    /* --- compute current total width: outer frame + column separators + column widths --- */
    int outer = (bs != SC_BORDER_NONE && !no_outer) ? 2 : 0;
    int seps = 0;
    for (size_t c = 0; c + 1 < table_data->column_count; c++) {
        int is_hcol = (table->opts.header.col && c == 0);
        if (bs != SC_BORDER_NONE && (!table->opts.borders.no_inner_v || is_hcol))
            seps++;
    }
    int current = outer + seps;
    for (size_t c = 0; c < table_data->column_count; c++) current += table->col_widths[c];

    int delta = table->opts.total_width - current;
    if (delta == 0) return;

    /* --- distribute delta evenly across flex (non-fixed) columns --- */
    int nflex = 0;
    for (size_t c = 0; c < table_data->column_count; c++)
        if (table_data->columns[c].opts.fixed_width == 0) nflex++;
    if (nflex == 0) return;

    /* integer division: give each flex column `per` cols, distribute remainder
       one col at a time (sign handles both shrink and grow) */
    int per = delta / nflex;
    int rem = delta - per * nflex;
    int sign = (rem >= 0) ? 1 : -1;
    rem = (rem < 0) ? -rem : rem;

    for (size_t c = 0; c < table_data->column_count; c++) {
        if (table_data->columns[c].opts.fixed_width > 0) continue;
        int adj = per + (rem > 0 ? sign : 0);
        if (rem > 0) rem--;
        int new_w = table->col_widths[c] + adj;
        if (new_w < 2) new_w = 2;
        table->col_widths[c] = new_w;
    }
}

/* Sum of all column widths plus the inner vertical separators between them. */
static int table_inner_w(const Table *table) {
    const ScTableData *table_data = table->table_data;
    int w = 0;
    for (size_t c = 0; c < table_data->column_count; c++) {
        /* --- add this column's width --- */
        w += table->col_widths[c];
        /* --- add the separator after this column, if one exists --- */
        if (c < table_data->column_count - 1 && table->opts.borders.style != SC_BORDER_NONE) {
            int is_hcol = (table->opts.header.col && c == 0);
            if (!table->opts.borders.no_inner_v || is_hcol) w++;
        }
    }
    return w;
}

/* Pre-compute the visual height of each data row (spanning cells excluded). */
static int *compute_row_heights(const Table *table) {
    const ScTableData *table_data = table->table_data;
    int cpy   = table->opts.cell_pad.top + table->opts.cell_pad.bottom;
    int cpx_v = table->opts.cell_pad.left + table->opts.cell_pad.right;
    int *rh   = malloc(table_data->row_count * sizeof(int));
    for (size_t r = 0; r < table_data->row_count; r++) {

        /* --- find the tallest non-spanning cell in this row --- */
        size_t max_c = 0;
        for (size_t c = 0; c < table_data->column_count; c++) {
            const ScCell *cell = &table_data->rows[r].cells[c];
            /* skip colspan-covered and rowspan-continuation cells */
            if (cell->colspan < 0) continue;
            if (cell->rowspan < 0 || cell->rowspan > 1) continue;
            /* build (or wrap) cell lines and count them */
            int cw_c = table->col_widths[c] - cpx_v;
            if (cw_c < 0) cw_c = 0;
            size_t nl;
            TLine *lines = (table_data->columns[c].opts.word_wrap && cw_c > 0)
                ? wrap_cell_lines(cell, cw_c, &nl)
                : make_cell_lines(cell, &nl);
            free_tlines(lines, nl);
            if (nl > max_c) max_c = nl;
        }

        /* --- row height = tallest cell content + vertical cell padding --- */
        rh[r] = (int)max_c + cpy;
        if (rh[r] < 1) rh[r] = 1;
    }
    return rh;
}

static void table_init(
    Table *table, const ScTableData *table_data, ScTableOpts opts
) {
    /* --- store inputs and allocate per-column arrays --- */
    table->table_data  = table_data;
    table->opts        = opts;
    table->col_widths  = malloc(table_data->column_count * sizeof(int));
    table->is_rs       = calloc(table_data->column_count, sizeof(int));
    table->rsc         = calloc(table_data->column_count, sizeof(RSCtx));

    /* --- compute layout: column widths → total-width scaling → inner width → row heights --- */
    compute_col_widths(table);
    apply_total_width(table);
    table->inner_w     = table_inner_w(table);
    table->row_heights = compute_row_heights(table);
}
