#pragma once

#include "sparcli.h"
#include "internal.h"
#include "table_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void print_ch          (const char *s, ScColor fg);
static void print_spaces_bg   (int n, ScColor bg);
static void print_span_bg     (const char *text, ScTextStyle opts, ScColor cell_bg);
static void tpre              (const Table *table);
static void print_empty_lines (int n);
static void render_horizontal_border(
    const Table *table,
    const char *lc,  const char *rc,
    const char *fill, ScColor fill_color,
    const char *mid,  ScColor mid_color,
    ScColor edge_color, int use_hcol, const int *rs
);
static void adjust_hborder_corners (const Table *table, const int *rs, const char **lc, const char **rc);
static void render_hborder_col_fill(const Table *table, size_t c, const int *rs, const char *fill, ScColor fill_color);
static void render_hborder_junction(const Table *table, size_t c, const int *rs, const char *mid, ScColor mid_color, int use_hcol);
static void render_inner_sep         (Table *table);
static void render_isep_span_col     (Table *table, size_t c);
static void render_isep_span_content (Table *table, size_t c, ScColor col_bg);
static int  compute_span_cli         (const RSCtx *rsc, int cn);
static void print_tline_in_width     (const TLine *line, int width, ScHAlign ha, ScColor bg);
static void render_isep_border_fill  (Table *table, size_t c);
static void render_isep_junction     (Table *table, size_t c);
static void render_title_line        (const Table *table, int is_top);
static void render_title_inner       (const Table *table, const char *h, ScColor oc, int tpad);
static void render_title_with_fill   (const Table *table, const char *h, ScColor oc, int tpad);
static void compute_title_fill_split (int inner_w, int tlen, int tpad, ScHAlign align, int *ld, int *rd);
static void render_title_full_fill   (int inner_w, const char *h, ScColor oc);


/* ── Low-level print helpers ─────────────────────────────────────────────── */

/* Prints a single string `s` in foreground color `fg`, then resets. */
static void print_ch(const char *s, ScColor fg) {
    sc_apply_colors(fg, SC_ANSI_COLOR_NONE);
    fputs(s, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

/* Prints `n` space characters with background color `bg` applied, then resets. */
static void print_spaces_bg(int n, ScColor bg) {
    if (n <= 0) return;
    if (bg.index != -2) sc_apply_colors(SC_ANSI_COLOR_NONE, bg);
    for (int i = 0; i < n; i++) fputc(' ', stdout);
    if (bg.index != -2) fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

/* Prints a styled span, falling back to the cell background when the span
   has no background of its own. */
static void print_span_bg(const char *text, ScTextStyle opts, ScColor cell_bg) {
    if (opts.bg.index == -2 && cell_bg.index != -2) opts.bg = cell_bg;
    sc_print(text, opts);
}

/* Prints the left margin spaces for the table. */
static void tpre(const Table *table) {
    for (int i = 0; i < table->opts.margin.left; i++) fputc(' ', stdout);
}

/* Prints `n` empty lines to apply vertical margins. */
static void print_empty_lines(int n) {
    for (int i = 0; i < n; i++) {
        fputc('\n', stdout);
    }
}

/* ── Horizontal border rendering ─────────────────────────────────────────── */

/** Renders one horizontal border line (top, bottom, or inner separator).
 *  Adjusts edge corners for active rowspans, then prints margin, left edge,
 *  per-column fill and junction chars, right edge, and newline. */
static void render_horizontal_border(
    const Table *table,
    const char *lc,  const char *rc,
    const char *fill, ScColor fill_color,
    const char *mid,  ScColor mid_color,
    ScColor edge_color, int use_hcol,
    const int *rs   /* rs[c]=1 → col c has active rowspan */
) {
    const ScTableData *table_data = table->table_data;
    int rtl      = table->opts.rtl;
    int no_outer = table->opts.border.no_outer;

    if (rs && !no_outer)
        adjust_hborder_corners(table, rs, &lc, &rc);

    tpre(table);
    if (!no_outer) print_ch(lc, edge_color);

    for (size_t ci = 0; ci < table_data->column_count; ci++) {
        size_t c = rtl ? (table_data->column_count - 1 - ci) : ci;
        render_hborder_col_fill(table, c, rs, fill, fill_color);
        if (ci < table_data->column_count - 1 && mid)
            render_hborder_junction(table, c, rs, mid, mid_color, use_hcol);
    }

    if (!no_outer) print_ch(rc, edge_color);
    fputc('\n', stdout);
}

/** Replaces @p *lc / @p *rc with the vertical-bar glyph when the first or
 *  last rendered column has an active rowspan. */
static void adjust_hborder_corners(
    const Table *table, const int *rs,
    const char **lc, const char **rc
) {
    const ScTableData *table_data = table->table_data;
    ScBorderType bs = table->opts.border.style;
    int rtl         = table->opts.rtl;
    size_t fc  = rtl ? (table_data->column_count - 1) : 0;
    size_t lcc = rtl ? 0 : (table_data->column_count - 1);
    if (rs[fc])  *lc = tbc[bs].v;
    if (rs[lcc]) *rc = tbc[bs].v;
}

/** Prints the fill segment for column @p c: spaces when a rowspan is active,
 *  otherwise the repeated @p fill character in @p fill_color. */
static void render_hborder_col_fill(
    const Table *table, size_t c, const int *rs,
    const char *fill, ScColor fill_color
) {
    if (rs && rs[c]) {
        for (int i = 0; i < table->column_widths[c]; i++) fputc(' ', stdout);
    } else {
        sc_apply_colors(fill_color, SC_ANSI_COLOR_NONE);
        for (int i = 0; i < table->column_widths[c]; i++) fputs(fill, stdout);
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    }
}

/** Prints the junction character between column @p c and its neighbour.
 *  Selects the glyph based on which side has an active rowspan, and overrides
 *  the color for the header-column separator when @p use_hcol is set. */
static void render_hborder_junction(
    const Table *table, size_t c,
    const int *rs,
    const char *mid, ScColor mid_color, int use_hcol
) {
    const ScTableData *table_data = table->table_data;
    ScBorderType bs  = table->opts.border.style;
    int rtl          = table->opts.rtl;
    size_t hcol_phys = rtl ? (table_data->column_count - 1) : 0;
    int is_hcol      = (table->opts.header.col && c == hcol_phys);
    int has_vsep     = !table->opts.border.no_inner_v || is_hcol;
    if (!has_vsep) return;

    const char *jchar = mid;
    if (rs) {
        size_t nc   = rtl ? (c - 1) : (c + 1);
        int cur_rs  = rs[c];
        int next_rs = rs[nc];
        if      (cur_rs && next_rs) jchar = tbc[bs].v;
        else if (cur_rs)            jchar = tbc[bs].t_left;
        else if (next_rs)           jchar = tbc[bs].t_right;
    }
    ScColor jc = mid_color;
    if (use_hcol && is_hcol && table->opts.border.header_col_sep_color.index != -2)
        jc = table->opts.border.header_col_sep_color;
    print_ch(jchar, jc);
}

/* ── Inner row separator ─────────────────────────────────────────────────── */

/** Renders the horizontal separator line between two data rows. Spanning columns
 *  continue their cell content; all others draw the inner border fill character. */
static void render_inner_sep(Table *table) {
    const ScTableData *table_data = table->table_data;
    const int *is_rs = table->is_rs;
    ScBorderType bs   = table->opts.border.style;
    ScColor oc        = table->opts.border.outer_color;
    int no_outer      = table->opts.border.no_outer;
    int rtl           = table->opts.rtl;

    const char *lc = tbc[bs].t_left;
    const char *rc = tbc[bs].t_right;
    if (is_rs && !no_outer)
        adjust_hborder_corners(table, is_rs, &lc, &rc);

    tpre(table);
    if (!no_outer) print_ch(lc, oc);

    for (size_t ci = 0; ci < table_data->column_count; ci++) {
        size_t c = rtl ? (table_data->column_count - 1 - ci) : ci;
        if (is_rs && is_rs[c])
            render_isep_span_col(table, c);
        else
            render_isep_border_fill(table, c);
        if (ci < table_data->column_count - 1)
            render_isep_junction(table, c);
    }

    if (!no_outer) print_ch(rc, oc);
    fputc('\n', stdout);
}

/** Renders one spanning column during an inner separator: either the next
 *  content line of the spanning cell, or a blank fill when there is no cell. */
static void render_isep_span_col(Table *table, size_t c) {
    ScColor col_bg = table->table_data->columns[c].opts.bg;
    if (table->rsc && table->rsc[c].cell)
        render_isep_span_content(table, c, col_bg);
    else
        print_spaces_bg(table->column_widths[c], col_bg);
}

/** Builds the spanning cell's lines, selects the correct visual line for the
 *  current separator row, and prints it between left and right cell padding. */
static void render_isep_span_content(Table *table, size_t c, ScColor col_bg) {
    const ScTableData *table_data = table->table_data;
    int cpxl      = table->opts.cell_pad.left;
    int cpxr      = table->opts.cell_pad.right;
    int content_w = table->column_widths[c] - cpxl - cpxr;
    if (content_w < 0) content_w = 0;

    size_t ncl;
    TLine *cl = (table_data->columns[c].opts.word_wrap && content_w > 0)
        ? wrap_cell_lines(table->rsc[c].cell, content_w, &ncl)
        : make_cell_lines(table->rsc[c].cell, &ncl);

    int cli = compute_span_cli(&table->rsc[c], (int)ncl);

    print_spaces_bg(cpxl, col_bg);
    if (cli >= 0 && cli < (int)ncl)
        print_tline_in_width(&cl[cli], content_w, table_data->columns[c].opts.halign, col_bg);
    else
        print_spaces_bg(content_w, col_bg);
    print_spaces_bg(cpxr, col_bg);

    free_tlines(cl, ncl);
}

/** Returns the visual line index to render for a spanning cell at the current
 *  separator row, based on vertical alignment and remaining blank space. */
static int compute_span_cli(const RSCtx *rsc, int cn) {
    int extra = rsc->span_h - cn;
    if (extra < 0) extra = 0;
    int top = 0;
    if      (rsc->valign == SC_VALIGN_MIDDLE) top = extra / 2;
    else if (rsc->valign == SC_VALIGN_BOTTOM) top = extra;
    return rsc->vis_offset - top;
}

/** Prints @p line within @p width columns: adds alignment padding when the
 *  content fits, or truncates span by span when it is wider than @p width. */
static void print_tline_in_width(const TLine *line, int width, ScHAlign ha, ScColor bg) {
    int rw = width - (int)line->vis_w;
    if (rw >= 0) {
        int lp = 0, rp = rw;
        if      (ha == SC_ALIGN_CENTER) { lp = rw/2; rp = rw - lp; }
        else if (ha == SC_ALIGN_RIGHT)  { lp = rw;   rp = 0; }
        print_spaces_bg(lp, bg);
        for (size_t s = 0; s < line->count; s++)
            print_span_bg(line->spans[s].text, line->spans[s].opts, bg);
        print_spaces_bg(rp, bg);
    } else {
        int rem = width;
        for (size_t s = 0; s < line->count && rem > 0; s++) {
            const char *txt = line->spans[s].text;
            int sw = (int)sc_utf8_string_length(txt, strlen(txt));
            if (sw <= rem) {
                print_span_bg(txt, line->spans[s].opts, bg);
                rem -= sw;
            } else {
                size_t bytes = sc_utf8_trim_to_cols(txt, rem);
                char *part = strndup(txt, bytes);
                print_span_bg(part, line->spans[s].opts, bg);
                free(part); rem = 0;
            }
        }
    }
}

/** Prints the inner border fill for a normal (non-spanning) column. */
static void render_isep_border_fill(Table *table, size_t c) {
    ScBorderType bs = table->opts.border.style;
    ScColor ic      = table->opts.border.inner_color;
    sc_apply_colors(ic, SC_ANSI_COLOR_NONE);
    for (int i = 0; i < table->column_widths[c]; i++) fputs(tbc[bs].h, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

/** Prints the junction character between column @p c and its neighbour,
 *  selecting the glyph based on active rowspans on either side. */
static void render_isep_junction(Table *table, size_t c) {
    const ScTableData *table_data = table->table_data;
    const int *is_rs = table->is_rs;
    ScBorderType bs  = table->opts.border.style;
    ScColor ic       = table->opts.border.inner_color;
    int rtl          = table->opts.rtl;
    size_t hcol_phys = rtl ? (table_data->column_count - 1) : 0;
    int is_hcol      = (table->opts.header.col && c == hcol_phys);
    int has_vsep     = !table->opts.border.no_inner_v || is_hcol;
    if (!has_vsep) return;

    size_t nc   = rtl ? (c - 1) : (c + 1);
    int cur_rs  = is_rs ? is_rs[c]       : 0;
    int next_rs = is_rs ? (int)is_rs[nc] : 0;
    const char *jchar = tbc[bs].cross;
    if      (cur_rs && next_rs) jchar = tbc[bs].v;
    else if (cur_rs)            jchar = tbc[bs].t_left;
    else if (next_rs)           jchar = tbc[bs].t_right;
    ScColor jc = ic;
    if (is_hcol && table->opts.border.header_col_sep_color.index != -2)
        jc = table->opts.border.header_col_sep_color;
    print_ch(jchar, jc);
}

/* ── Title line ──────────────────────────────────────────────────────────── */

/** Renders the top or bottom border line with the table title embedded in it.
 *  Prints the left/right corner chars and delegates the inner fill to
 *  render_title_inner(). */
static void render_title_line(const Table *table, int is_top) {
    ScBorderType bs = table->opts.border.style;
    ScColor oc      = table->opts.border.outer_color;
    const char *lc  = is_top ? tbc[bs].tl : tbc[bs].bl;
    const char *rc  = is_top ? tbc[bs].tr : tbc[bs].br;
    const char *h   = tbc[bs].h;
    int tpad        = table->opts.title.pad > 0 ? table->opts.title.pad : 1;

    tpre(table);
    print_ch(lc, oc);
    render_title_inner(table, h, oc, tpad);
    print_ch(rc, oc);
    fputc('\n', stdout);
}

/** Dispatches between title-present and no-title rendering for the inner fill
 *  area of a title border line. */
static void render_title_inner(const Table *table, const char *h, ScColor oc, int tpad) {
    if (table->opts.title.text && *table->opts.title.text)
        render_title_with_fill(table, h, oc, tpad);
    else
        render_title_full_fill(table->inner_w, h, oc);
}

/** Computes the left/right fill counts and prints: fill, padding, title text,
 *  padding, fill — all in the outer-color / title-style combination. */
static void render_title_with_fill(const Table *table, const char *h, ScColor oc, int tpad) {
    int tlen = (int)strlen(table->opts.title.text);
    int ld, rd;
    compute_title_fill_split(table->inner_w, tlen, tpad, table->opts.title.align, &ld, &rd);

    sc_apply_colors(oc, SC_ANSI_COLOR_NONE);
    for (int i = 0; i < ld; i++) fputs(h, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    for (int i = 0; i < tpad; i++) print_ch(" ", oc);
    sc_print(table->opts.title.text, table->opts.title.style);
    for (int i = 0; i < tpad; i++) print_ch(" ", oc);
    sc_apply_colors(oc, SC_ANSI_COLOR_NONE);
    for (int i = 0; i < rd; i++) fputs(h, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

/** Computes the number of fill characters to the left (@p ld) and right (@p rd)
 *  of the title text, distributing them according to @p align. */
static void compute_title_fill_split(int inner_w, int tlen, int tpad,
                                      ScHAlign align, int *ld, int *rd) {
    int dashes = inner_w - tlen - 2 * tpad;
    if (dashes < 0) dashes = 0;
    *ld = 1; *rd = dashes - 1;
    if      (align == SC_ALIGN_CENTER) { *ld = dashes/2; *rd = dashes - *ld; }
    else if (align == SC_ALIGN_RIGHT)  { *ld = dashes - 1; *rd = 1; }
    if (*ld < 0) *ld = 0;
    if (*rd < 0) *rd = 0;
}

/** Fills the entire @p inner_w of the title line with the border fill character
 *  when no title text is set. */
static void render_title_full_fill(int inner_w, const char *h, ScColor oc) {
    sc_apply_colors(oc, SC_ANSI_COLOR_NONE);
    for (int i = 0; i < inner_w; i++) fputs(h, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}
