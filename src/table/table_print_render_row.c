#pragma once

#include "sparcli.h"
#include "internal.h"
#include "table_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "table_print_render_cell.c"
#include "table_print_render_border.c"

/* Per-row render context: derived once from table + row_kind, passed through
   the rendering stack to avoid recomputing shared state at every call site. */
typedef struct {
    const RSCtx *rsc;      /* rowspan contexts; NULL for header/footer */
    ScColor      row_bg;   /* resolved row background */
    int          row_kind; /* 0=data, 1=header, 2=footer */
    int          is_hdr;   /* row_kind == 1 */
    int          is_ftr;   /* row_kind == 2 */
    int          row_h;    /* total visual height of this row */
    int          cpxl;     /* cell_pad.left */
    int          cpxr;     /* cell_pad.right */
    int          cpy;      /* cell_pad.top + cell_pad.bottom */
    int          cpt;      /* cell_pad.top */
} RowRCtx;

static void    render_row                   (const Table *table, const ScCell *cells, ScColor row_bg, int row_kind, int row_h_in);
static size_t  build_row_cell_lines         (const Table *table, const ScCell *cells, const RowRCtx *rctx, TLine **cl, size_t *cnl);
static void    build_rowspan_cont_col_lines (const Table *table, size_t c, int cpx, const RowRCtx *rctx, TLine **cl, size_t *cnl);
static size_t  build_span_col_lines         (const Table *table, const ScCell *cell, size_t c, int cpx, TLine **cl, size_t *cnl);
static void    render_row_visual_line       (const Table *table, const ScCell *cells, TLine **cl, size_t *cnl, int li, const RowRCtx *rctx);
static void    render_row_cell              (const Table *table, const ScCell *cells, TLine **cl, size_t *cnl, size_t c, size_t ci, int ecs, int li, const RowRCtx *rctx);
static int     compute_span_w               (const Table *table, size_t c, int ecs);
static ScColor resolve_cell_bg              (const Table *table, size_t c, ScColor row_bg, int row_kind);
static void    resolve_cell_align           (const Table *table, const ScCell *cells, size_t c, ScHAlign *ha, ScVAlign *va);
static int     compute_rowspan_cell_cli     (const RSCtx *rsc_c, int li, int cn);
static int     compute_normal_cell_cli      (int li, int cn, ScVAlign va, const RowRCtx *rctx);
static ScTextStyle apply_hdrftr_style       (ScTextStyle so, const ScTableOpts *opts, const RowRCtx *rctx);
static void    render_cell_line             (const TLine *line, int cw, ScHAlign ha, ScColor cell_bg, const ScTableOpts *opts, const RowRCtx *rctx);
static void    print_cell_line_aligned      (const TLine *line, int rw, ScHAlign ha, ScColor cell_bg, const ScTableOpts *opts, const RowRCtx *rctx);
static void    print_cell_line_truncated    (const TLine *line, int cw, ScColor cell_bg, const ScTableOpts *opts, const RowRCtx *rctx);
static void    render_row_vsep              (const Table *table, size_t c, size_t ci, int ecs);


/* ── Row rendering ───────────────────────────────────────────────────────── */

/** Initialises a RowRCtx, builds per-column TLine arrays, dispatches each
 *  visual line to render_row_visual_line(), then frees the arrays. */
static void render_row(const Table *table, const ScCell *cells,
                        ScColor row_bg, int row_kind, int row_h_in) {
    const ScTableData *table_data = table->table_data;

    TLine **cl  = malloc(table_data->column_count * sizeof(TLine *));
    size_t *cnl = malloc(table_data->column_count * sizeof(size_t));

    RowRCtx rctx = {
        .rsc      = (row_kind == 0) ? table->rsc : NULL,
        .row_bg   = row_bg,
        .row_kind = row_kind,
        .is_hdr   = (row_kind == 1),
        .is_ftr   = (row_kind == 2),
        .cpxl     = table->opts.cell_pad.left,
        .cpxr     = table->opts.cell_pad.right,
        .cpy      = table->opts.cell_pad.top + table->opts.cell_pad.bottom,
        .cpt      = table->opts.cell_pad.top,
    };

    size_t max_content = build_row_cell_lines(table, cells, &rctx, cl, cnl);
    rctx.row_h = row_h_in > 0 ? row_h_in : (int)max_content + rctx.cpy;
    if (rctx.row_h < 1) { rctx.row_h = 1; }

    for (int li = 0; li < rctx.row_h; li++) {
        render_row_visual_line(table, cells, cl, cnl, li, &rctx);
    }

    for (size_t c = 0; c < table_data->column_count; c++) {
        if (cl[c]) { free_tlines(cl[c], cnl[c]); }
    }
    free(cl);
    free(cnl);
}

/** Loops over every column, dispatching to build_rowspan_cont_col_lines() or
 *  build_span_col_lines(). Returns the tallest non-rowspan-starting line count. */
static size_t build_row_cell_lines(const Table *table, const ScCell *cells,
                                    const RowRCtx *rctx, TLine **cl, size_t *cnl) {
    const ScTableData *table_data = table->table_data;
    int rtl = table->opts.rtl;
    int cpx = rctx->cpxl + rctx->cpxr;
    size_t max_content = 0;

    for (size_t ci = 0; ci < table_data->column_count; ci++) {
        size_t c = rtl ? (table_data->column_count - 1 - ci) : ci;

        if (cells[c].colspan < 0) { cl[c] = NULL; cnl[c] = 0; continue; }

        if (cells[c].rowspan == -1) {
            build_rowspan_cont_col_lines(table, c, cpx, rctx, cl, cnl);
            continue;
        }

        size_t n = build_span_col_lines(table, &cells[c], c, cpx, cl, cnl);
        int rs_v = cells[c].rowspan;
        if ((rs_v == 0 || rs_v == 1) && n > max_content) {
            max_content = n;
        }
    }

    return max_content;
}

/** Builds the TLine array for column @p c that continues a rowspan from a
 *  previous row. Uses the originating cell from rctx->rsc; sets cl[c]=NULL
 *  when no active span context exists. */
static void build_rowspan_cont_col_lines(const Table *table, size_t c, int cpx,
                                          const RowRCtx *rctx, TLine **cl, size_t *cnl) {
    if (rctx->rsc && rctx->rsc[c].cell) {
        const ScTableData *table_data = table->table_data;
        int span_cw = table->column_widths[c] - cpx;
        if (span_cw < 0) { span_cw = 0; }
        if (table_data->columns[c].opts.word_wrap && span_cw > 0) {
            cl[c] = wrap_cell_lines(rctx->rsc[c].cell, span_cw, &cnl[c]);
        } else {
            cl[c] = make_cell_lines(rctx->rsc[c].cell, &cnl[c]);
        }
    } else {
        cl[c] = NULL; cnl[c] = 0;
    }
}

/** Builds the TLine array for a normal or colspan-initiating cell at column @p c.
 *  Selects word-wrap or plain building based on column settings.
 *  Returns the content line count written to cnl[c]. */
static size_t build_span_col_lines(const Table *table, const ScCell *cell,
                                    size_t c, int cpx, TLine **cl, size_t *cnl) {
    const ScTableData *table_data = table->table_data;
    int cs  = cell->colspan;
    int ecs = (cs > 1) ? cs : 1;
    if ((int)c + ecs > (int)table_data->column_count) {
        ecs = (int)table_data->column_count - (int)c;
    }

    int cw = compute_span_w(table, c, ecs) - cpx;
    if (cw < 0) { cw = 0; }

    if (table_data->columns[c].opts.word_wrap && cw > 0) {
        cl[c] = wrap_cell_lines(cell, cw, &cnl[c]);
    } else {
        cl[c] = make_cell_lines(cell, &cnl[c]);
    }

    return cnl[c];
}

/** Renders one visual line @p li of the row: prints the left border, loops
 *  over columns via render_row_cell(), then closes with the right border. */
static void render_row_visual_line(const Table *table, const ScCell *cells,
                                    TLine **cl, size_t *cnl,
                                    int li, const RowRCtx *rctx) {
    const ScTableData *table_data = table->table_data;
    ScBorderType bs = table->opts.border.style;
    ScColor oc      = table->opts.border.outer_color;
    int no_outer    = table->opts.border.no_outer;
    int rtl         = table->opts.rtl;

    tpre(table);
    if (!no_outer) { print_ch(tbc[bs].v, oc); }

    size_t ci = 0;
    while (ci < table_data->column_count) {
        size_t c = rtl ? (table_data->column_count - 1 - ci) : ci;
        int cs_raw = cells[c].colspan;
        if (cs_raw < 0) { ci++; continue; }

        int ecs = (cs_raw > 1) ? cs_raw : 1;
        if ((int)c + ecs > (int)table_data->column_count) {
            ecs = (int)table_data->column_count - (int)c;
        }

        render_row_cell(table, cells, cl, cnl, c, ci, ecs, li, rctx);
        ci += (size_t)ecs;
    }

    if (!no_outer) { print_ch(tbc[bs].v, oc); }
    fputc('\n', stdout);
}

/** Renders one cell (padding + content + separator) at visual line @p li.
 *  Resolves background, alignment, and content line index, then delegates. */
static void render_row_cell(const Table *table, const ScCell *cells,
                             TLine **cl, size_t *cnl,
                             size_t c, size_t ci, int ecs, int li,
                             const RowRCtx *rctx) {
    int cw = compute_span_w(table, c, ecs) - (rctx->cpxl + rctx->cpxr);
    if (cw < 0) { cw = 0; }

    ScColor  cell_bg = resolve_cell_bg(table, c, rctx->row_bg, rctx->row_kind);
    ScHAlign ha; ScVAlign va;
    resolve_cell_align(table, cells, c, &ha, &va);

    int cn        = cl[c] ? (int)cnl[c] : 0;
    int rs_render = cells[c].rowspan;
    int cli;
    if ((rs_render > 1 || rs_render == -1) && rctx->rsc && rctx->rsc[c].cell) {
        cli = compute_rowspan_cell_cli(&rctx->rsc[c], li, cn);
    } else {
        cli = compute_normal_cell_cli(li, cn, va, rctx);
    }

    print_spaces_bg(rctx->cpxl, cell_bg);
    if (cli >= 0 && cli < cn && cl[c]) {
        render_cell_line(&cl[c][cli], cw, ha, cell_bg, &table->opts, rctx);
    } else {
        print_spaces_bg(cw, cell_bg);
    }
    print_spaces_bg(rctx->cpxr, cell_bg);

    render_row_vsep(table, c, ci, ecs);
}

/** Computes the raw column width for a colspan span, absorbing inner vertical
 *  separator characters into the total. */
static int compute_span_w(const Table *table, size_t c, int ecs) {
    const ScTableData *table_data = table->table_data;
    int rtl        = table->opts.rtl;
    int no_inner_v = table->opts.border.no_inner_v;
    size_t hcol_phys = rtl ? (table_data->column_count - 1) : 0;

    int span_total = 0;
    for (int k = 0; k < ecs; k++) {
        size_t cc = c + (size_t)k;
        span_total += table->column_widths[cc];
        if (k < ecs - 1) {
            int is_hcol_k = (table->opts.header.col && cc == hcol_phys);
            if (!no_inner_v || is_hcol_k) { span_total++; }
        }
    }
    return span_total;
}

/** Resolves the effective cell background applying priority:
 *  header/footer row/col bg > explicit row bg > column bg. */
static ScColor resolve_cell_bg(const Table *table, size_t c,
                                ScColor row_bg, int row_kind) {
    const ScTableData *table_data = table->table_data;
    int rtl      = table->opts.rtl;
    int is_hdr   = (row_kind == 1);
    int is_ftr   = (row_kind == 2);
    size_t hcol_phys = rtl ? (table_data->column_count - 1) : 0;

    ScColor col_bg  = table_data->columns[c].opts.bg;
    ScColor cell_bg = (row_bg.index != -2) ? row_bg : col_bg;
    int is_hcol_c   = (table->opts.header.col && c == hcol_phys);

    if (is_hcol_c && !is_hdr) {
        cell_bg = is_ftr ? table->opts.footer.col_bg : table->opts.header.col_bg;
    }
    if (is_hdr)               { cell_bg = table->opts.header.row_bg; }
    if (is_ftr && !is_hcol_c) { cell_bg = table->opts.footer.row_bg; }
    if (is_ftr &&  is_hcol_c) { cell_bg = table->opts.footer.col_bg; }

    return cell_bg;
}

/** Resolves the horizontal and vertical alignment for column @p c,
 *  applying per-cell overrides on top of the column defaults. */
static void resolve_cell_align(const Table *table, const ScCell *cells, size_t c,
                                ScHAlign *ha, ScVAlign *va) {
    const ScTableData *table_data = table->table_data;
    *ha = table_data->columns[c].opts.halign;
    *va = table_data->columns[c].opts.valign;
    if (cells[c].align_set)  { *ha = cells[c].align; }
    if (cells[c].valign_set) { *va = cells[c].valign; }
}

/** Computes the content line index for a rowspan cell at visual line @p li,
 *  distributing lines across the full span height with vertical alignment. */
static int compute_rowspan_cell_cli(const RSCtx *rsc_c, int li, int cn) {
    int extra = rsc_c->span_h - cn;
    if (extra < 0) { extra = 0; }
    int top = 0;
    if      (rsc_c->valign == SC_VALIGN_MIDDLE) { top = extra / 2; }
    else if (rsc_c->valign == SC_VALIGN_BOTTOM) { top = extra; }
    return rsc_c->vis_offset + li - top;
}

/** Computes the content line index for a normal (non-rowspan) cell at visual
 *  line @p li, applying vertical alignment within the row height. */
static int compute_normal_cell_cli(int li, int cn, ScVAlign va, const RowRCtx *rctx) {
    int extra = rctx->row_h - cn - rctx->cpy;
    if (extra < 0) { extra = 0; }
    int top_pad = rctx->cpt;
    if      (va == SC_VALIGN_MIDDLE) { top_pad += extra / 2; }
    else if (va == SC_VALIGN_BOTTOM) { top_pad += extra; }
    return li - top_pad;
}

/** Returns @p so with header or footer text style applied to unstyled spans:
 *  only sets attr/fg when the span carries none of its own. */
static ScTextStyle apply_hdrftr_style(ScTextStyle so, const ScTableOpts *opts,
                                       const RowRCtx *rctx) {
    if (rctx->is_hdr) {
        if (so.attr == 0)      { so.attr = opts->header.opts.attr; }
        if (so.fg.index == -2) { so.fg   = opts->header.opts.fg; }
    } else if (rctx->is_ftr) {
        if (so.attr == 0)      { so.attr = opts->footer.opts.attr; }
        if (so.fg.index == -2) { so.fg   = opts->footer.opts.fg; }
    }
    return so;
}

/** Dispatches to print_cell_line_aligned() when content fits within @p cw,
 *  or print_cell_line_truncated() when it is wider. */
static void render_cell_line(const TLine *line, int cw, ScHAlign ha, ScColor cell_bg,
                              const ScTableOpts *opts, const RowRCtx *rctx) {
    int rw = cw - (int)line->vis_w;
    if (rw >= 0) {
        print_cell_line_aligned(line, rw, ha, cell_bg, opts, rctx);
    } else {
        print_cell_line_truncated(line, cw, cell_bg, opts, rctx);
    }
}

/** Prints all spans of @p line with left/right alignment padding totalling @p rw. */
static void print_cell_line_aligned(const TLine *line, int rw, ScHAlign ha,
                                     ScColor cell_bg, const ScTableOpts *opts,
                                     const RowRCtx *rctx) {
    int lp = 0, rp = rw;
    if      (ha == SC_ALIGN_CENTER) { lp = rw/2; rp = rw - lp; }
    else if (ha == SC_ALIGN_RIGHT)  { lp = rw;   rp = 0;       }
    print_spaces_bg(lp, cell_bg);
    for (size_t s = 0; s < line->count; s++) {
        ScTextStyle so = apply_hdrftr_style(line->spans[s].opts, opts, rctx);
        print_span_bg(line->spans[s].text, so, cell_bg);
    }
    print_spaces_bg(rp, cell_bg);
}

/** Prints as many columns of @p line as fit within @p cw, truncating the last
 *  span at the exact column boundary when needed. */
static void print_cell_line_truncated(const TLine *line, int cw, ScColor cell_bg,
                                       const ScTableOpts *opts, const RowRCtx *rctx) {
    int remaining = cw;
    for (size_t s = 0; s < line->count && remaining > 0; s++) {
        ScTextStyle so  = apply_hdrftr_style(line->spans[s].opts, opts, rctx);
        const char *txt = line->spans[s].text;
        int sw = (int)sc_utf8_string_length(txt, strlen(txt));
        if (sw <= remaining) {
            print_span_bg(txt, so, cell_bg);
            remaining -= sw;
        } else {
            size_t bytes = sc_utf8_trim_to_cols(txt, remaining);
            char *part = strndup(txt, bytes);
            print_span_bg(part, so, cell_bg);
            free(part); remaining = 0;
        }
    }
}

/** Prints the vertical separator after the cell at column @p c, selecting
 *  the header-column separator color when applicable. */
static void render_row_vsep(const Table *table, size_t c, size_t ci, int ecs) {
    const ScTableData *table_data = table->table_data;
    ScBorderType bs  = table->opts.border.style;
    ScColor ic       = table->opts.border.inner_color;
    int no_inner_v   = table->opts.border.no_inner_v;
    int rtl          = table->opts.rtl;
    size_t hcol_phys = rtl ? (table_data->column_count - 1) : 0;

    size_t end_col = c + (size_t)ecs - 1;
    if (ci + (size_t)ecs < table_data->column_count && bs != SC_BORDER_NONE) {
        int is_hcol_end = (table->opts.header.col && end_col == hcol_phys);
        if (!no_inner_v || is_hcol_end) {
            ScColor sc_col = ic;
            if (is_hcol_end && table->opts.border.header_col_sep_color.index != -2) {
                sc_col = table->opts.border.header_col_sep_color;
            }
            print_ch(tbc[bs].v, sc_col);
        }
    }
}
