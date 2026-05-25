#pragma once

#include "sparcli.h"         // IWYU pragma: export
#include "internal.h"        // IWYU pragma: export
#include "table_internal.h"  // IWYU pragma: export
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


static void table_render(Table *table);
    static void print_empty_lines(int n);
    static void render_top_border    (const Table *table);
    static void render_header_row    (const Table *table);
    static void render_data_rows     (Table *table);
    static void render_footer_rows   (const Table *table);
    static void render_bottom_border (const Table *table);
    static void tpre(const Table *table);
    static void print_ch       (const char *s, ScColor fg);
    static void print_spaces_bg(int n, ScColor bg);
    static void print_span_bg  (const char *text, ScTextStyle opts, ScColor cell_bg);
    static void render_horizontal_border(
        const Table *table,
        const char *lc,  const char *rc,
        const char *fill, ScColor fill_color,
        const char *mid,  ScColor mid_color,
        ScColor edge_color, int use_hcol, const int *rs
    );
    static void render_inner_sep (Table *table);
    static void render_title_line(const Table *table, int is_top);
    static void render_row(
        const Table *table, const ScCell *cells,
        ScColor row_bg, int row_kind, int row_h_in
    );
        static void    free_tlines    (TLine *lines, size_t n);
        static void    flush_tline    (TLine **lines, size_t *cap, size_t *n,
                                       TSpan *buf, size_t buf_n, size_t vis_w);
        static TLine  *make_cell_lines(const ScCell *cell, size_t *out_n);
        static size_t  cell_vis_width (const ScCell *cell);
        static TLine  *wrap_one_tline (const TLine *src, int wrap_w, size_t *out_n);
        static TLine  *wrap_cell_lines(const ScCell *cell, int wrap_w, size_t *out_n);


static void table_render(Table *table) {
    print_empty_lines(table->opts.margin.top);
    render_top_border(table);
    render_header_row(table);
    render_data_rows(table);
    render_footer_rows(table);
    render_bottom_border(table);
    print_empty_lines(table->opts.margin.bottom);
}



static void free_tlines(TLine *lines, size_t n) {
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < lines[i].count; j++)
            free((char *)lines[i].spans[j].text);
        free(lines[i].spans);
    }
    free(lines);
}

static void flush_tline(TLine **lines, size_t *cap, size_t *n,
                         TSpan *buf, size_t buf_n, size_t vis_w) {
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 4;
        *lines = realloc(*lines, *cap * sizeof(TLine));
    }
    TSpan *ls = malloc((buf_n + 1) * sizeof(TSpan));
    if (buf_n) memcpy(ls, buf, buf_n * sizeof(TSpan));
    (*lines)[(*n)++] = (TLine){ ls, buf_n, vis_w };
}

/* Build TLine array by splitting cell content on '\n'. */
static TLine *make_cell_lines(const ScCell *cell, size_t *out_n) {
    static const ScTextStyle PLAIN = { 0, { -2,0,0,0 }, { -2,0,0,0 } };

    size_t lines_cap = 4, nlines = 0;
    TLine *lines = malloc(lines_cap * sizeof(TLine));
    size_t buf_cap = 4, buf_n = 0, buf_w = 0;
    TSpan *buf = malloc(buf_cap * sizeof(TSpan));

    size_t nspans = ((cell->kind == SC_CELL_TEXT || cell->kind == SC_CELL_MARKUP) && cell->text) ? cell->text->count : 1;

    for (size_t si = 0; si < nspans; si++) {
        const char *s;
        ScTextStyle   opts;

        if (cell->kind == SC_CELL_STR) {
            s    = cell->str ? cell->str : "";
            opts = PLAIN;
        } else {
            s    = cell->text->spans[si].raw_str;
            opts = cell->text->spans[si].style;
        }

        const char *start = s;
        while (*s) {
            if (*s == '\n') {
                size_t len = (size_t)(s - start);
                if (len > 0) {
                    if (buf_n == buf_cap) { buf_cap *= 2; buf = realloc(buf, buf_cap * sizeof(TSpan)); }
                    buf[buf_n++] = (TSpan){ strndup(start, len), opts };
                    buf_w += sc_utf8_string_length(start, len);
                }
                flush_tline(&lines, &lines_cap, &nlines, buf, buf_n, buf_w);
                buf_n = 0; buf_w = 0;
                start = s + 1;
            }
            s++;
        }
        size_t len = (size_t)(s - start);
        if (len > 0) {
            if (buf_n == buf_cap) { buf_cap *= 2; buf = realloc(buf, buf_cap * sizeof(TSpan)); }
            buf[buf_n++] = (TSpan){ strndup(start, len), opts };
            buf_w += sc_utf8_string_length(start, len);
        }
    }

    flush_tline(&lines, &lines_cap, &nlines, buf, buf_n, buf_w);
    free(buf);
    *out_n = nlines;
    return lines;
}

static size_t cell_vis_width(const ScCell *cell) {
    size_t n;
    TLine *lines = make_cell_lines(cell, &n);
    size_t max_w = 0;
    for (size_t i = 0; i < n; i++)
        if (lines[i].vis_w > max_w) max_w = lines[i].vis_w;
    free_tlines(lines, n);
    return max_w;
}

/* ── Word-wrap ───────────────────────────────────────────────────────────── */

/* Wrap a single TLine's spans into multiple TLines fitting within wrap_w cols.
   Wraps at spaces (word-wrap); hard-breaks words longer than wrap_w. */
static TLine *wrap_one_tline(const TLine *src, int wrap_w, size_t *out_n) {
    /* Token: a "word" or "space-run" from a single span */
    typedef struct { char *s; size_t vis_w; ScTextStyle opts; int is_space; } Tok;
    Tok *toks = NULL; size_t ntok = 0, tcap = 0;

    for (size_t si = 0; si < src->count; si++) {
        const char *p = src->spans[si].text;
        ScTextStyle opts = src->spans[si].opts;
        while (*p) {
            const char *seg = p;
            int is_sp = (*p == ' ');
            while (*p && ((*p == ' ') == is_sp)) p++;
            size_t len = (size_t)(p - seg);
            size_t vw  = is_sp ? len : sc_utf8_string_length(seg, len);
            if (ntok == tcap) { tcap = tcap ? tcap * 2 : 16; toks = realloc(toks, tcap * sizeof(Tok)); }
            toks[ntok++] = (Tok){ strndup(seg, len), vw, opts, is_sp };
        }
    }

    TLine *res = NULL; size_t nres = 0, rcap = 0;
    TSpan *sbuf = NULL; size_t sn = 0, sc2 = 0; size_t sw = 0;

    /* flush sbuf as a new TLine, trimming trailing space spans */
    #define WW_FLUSH() do { \
        /* trim trailing spaces */ \
        while (sn > 0 && sbuf[sn-1].text[0] == ' ') { \
            sw -= strlen(sbuf[sn-1].text); \
            free((char*)sbuf[--sn].text); \
        } \
        flush_tline(&res, &rcap, &nres, sbuf, sn, sw); \
        sn = 0; sw = 0; \
    } while(0)

    for (size_t ti = 0; ti < ntok; ti++) {
        Tok *tok = &toks[ti];
        if (tok->is_space) {
            if (sn == 0) { free(tok->s); tok->s = NULL; continue; } /* skip leading spaces */
            /* peek: if next word doesn't fit after this space, wrap now */
            if (ti + 1 < ntok && !toks[ti+1].is_space
                    && (int)(sw + tok->vis_w + toks[ti+1].vis_w) > wrap_w) {
                free(tok->s); tok->s = NULL;
                WW_FLUSH();
                continue;
            }
            if (sn == sc2) { sc2 = sc2 ? sc2*2 : 8; sbuf = realloc(sbuf, sc2*sizeof(TSpan)); }
            sbuf[sn++] = (TSpan){ tok->s, tok->opts }; tok->s = NULL;
            sw += tok->vis_w;
        } else {
            /* word */
            if (sn > 0 && (int)(sw + tok->vis_w) > wrap_w) WW_FLUSH();

            if ((int)tok->vis_w > wrap_w) {
                /* hard-break: split word into wrap_w-sized chunks */
                const char *p = tok->s;
                size_t rem = strlen(tok->s);
                while (rem > 0) {
                    int avail = wrap_w - (int)sw;
                    if (avail <= 0) { WW_FLUSH(); avail = wrap_w; }
                    size_t cb = sc_utf8_trim_to_cols(p, avail);
                    if (cb == 0) break;
                    char *chunk = strndup(p, cb);
                    size_t cw2  = sc_utf8_string_length(p, cb);
                    if (sn == sc2) { sc2 = sc2?sc2*2:8; sbuf = realloc(sbuf, sc2*sizeof(TSpan)); }
                    sbuf[sn++] = (TSpan){ chunk, tok->opts };
                    sw += cw2; p += cb; rem -= cb;
                    if (rem > 0) WW_FLUSH();
                }
                free(tok->s); tok->s = NULL;
            } else {
                if (sn == sc2) { sc2 = sc2?sc2*2:8; sbuf = realloc(sbuf, sc2*sizeof(TSpan)); }
                sbuf[sn++] = (TSpan){ tok->s, tok->opts }; tok->s = NULL;
                sw += tok->vis_w;
            }
        }
    }
    WW_FLUSH();
    #undef WW_FLUSH

    free(sbuf);
    for (size_t ti = 0; ti < ntok; ti++) if (toks[ti].s) free(toks[ti].s);
    free(toks);
    *out_n = nres;
    return res;
}

/* Wrap all lines in a TLine array that exceed wrap_w. */
static TLine *wrap_cell_lines(const ScCell *cell, int wrap_w, size_t *out_n) {
    size_t nraw;
    TLine *raw = make_cell_lines(cell, &nraw);
    if (wrap_w <= 0) { *out_n = nraw; return raw; }

    TLine *res = NULL; size_t nres = 0, rcap = 0;
    for (size_t li = 0; li < nraw; li++) {
        if ((int)raw[li].vis_w <= wrap_w) {
            /* copy as-is */
            if (nres == rcap) { rcap = rcap?rcap*2:4; res = realloc(res, rcap*sizeof(TLine)); }
            TSpan *ls = malloc((raw[li].count + 1) * sizeof(TSpan));
            for (size_t j = 0; j < raw[li].count; j++)
                ls[j] = (TSpan){ strdup(raw[li].spans[j].text), raw[li].spans[j].opts };
            res[nres++] = (TLine){ ls, raw[li].count, raw[li].vis_w };
        } else {
            size_t nw;
            TLine *wrapped = wrap_one_tline(&raw[li], wrap_w, &nw);
            if (nres + nw > rcap) {
                while (rcap < nres + nw) rcap = rcap ? rcap*2 : 4;
                res = realloc(res, rcap * sizeof(TLine));
            }
            memcpy(res + nres, wrapped, nw * sizeof(TLine));
            free(wrapped); /* shallow: TLine structs moved, spans owned by res */
            nres += nw;
        }
    }
    free_tlines(raw, nraw);
    *out_n = nres;
    return res;
}

/* ── Low-level print helpers ─────────────────────────────────────────────── */

static void print_ch(const char *s, ScColor fg) {
    sc_apply_colors(fg, SC_ANSI_COLOR_NONE);
    fputs(s, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

static void print_spaces_bg(int n, ScColor bg) {
    if (n <= 0) return;
    if (bg.index != -2) sc_apply_colors(SC_ANSI_COLOR_NONE, bg);
    for (int i = 0; i < n; i++) fputc(' ', stdout);
    if (bg.index != -2) fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

static void print_span_bg(const char *text, ScTextStyle opts, ScColor cell_bg) {
    if (opts.bg.index == -2 && cell_bg.index != -2) opts.bg = cell_bg;
    sc_print(text, opts);
}

static void tpre(const Table *table) {
    for (int i = 0; i < table->opts.margin.left; i++) fputc(' ', stdout);
}

/** Prints `n` empty lines to apply vertical margins to the table. */
static void print_empty_lines(int n) {
    for (int i = 0; i < n; i++) {
        fputc('\n', stdout);
    }
}

/* ── Horizontal border rendering ─────────────────────────────────────────── */

static void render_horizontal_border(
    const Table *table,
    const char *lc,  const char *rc,
    const char *fill, ScColor fill_color,
    const char *mid,  ScColor mid_color,
    ScColor edge_color, int use_hcol,
    const int *rs   /* rs[c]=1 → col c has active rowspan */
) {
    const ScTableData *table_data = table->table_data;
    ScBorderType bs  = table->opts.borders.style;
    int rtl          = table->opts.rtl;
    int no_outer     = table->opts.borders.no_outer;

    /* Adjust outer corners when first/last rendered col has rowspan */
    if (rs && !no_outer) {
        size_t fc  = rtl ? (table_data->column_count - 1) : 0;
        size_t lcc = rtl ? 0 : (table_data->column_count - 1);
        if (rs[fc])  lc = tbc[bs].v;
        if (rs[lcc]) rc = tbc[bs].v;
    }

    tpre(table);
    if (!no_outer) print_ch(lc, edge_color);

    for (size_t ci = 0; ci < table_data->column_count; ci++) {
        size_t c = rtl ? (table_data->column_count - 1 - ci) : ci;

        if (rs && rs[c]) {
            for (int i = 0; i < table->col_widths[c]; i++) fputc(' ', stdout);
        } else {
            sc_apply_colors(fill_color, SC_ANSI_COLOR_NONE);
            for (int i = 0; i < table->col_widths[c]; i++) fputs(fill, stdout);
            fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
        }

        if (ci < table_data->column_count - 1 && mid) {
            size_t hcol_phys = rtl ? (table_data->column_count - 1) : 0;
            int is_hcol  = (table->opts.header.col && c == hcol_phys);
            int has_vsep = !table->opts.borders.no_inner_v || is_hcol;
            if (has_vsep) {
                /* Determine junction char considering rowspan on either side */
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
                if (use_hcol && is_hcol
                        && table->opts.borders.header_col_sep_color.index != -2)
                    jc = table->opts.borders.header_col_sep_color;
                print_ch(jchar, jc);
            }
        }
    }

    if (!no_outer) print_ch(rc, edge_color);
    fputc('\n', stdout);
}

/* Inner row separator that can show spanning-cell content in spanned columns. */
static void render_inner_sep(Table *table) {
    const ScTableData *table_data = table->table_data;
    const int *is_rs    = table->is_rs;
    const RSCtx *rsc    = table->rsc;
    ScBorderType bs    = table->opts.borders.style;
    ScColor oc          = table->opts.borders.outer_color;
    ScColor ic          = table->opts.borders.inner_color;
    int no_outer        = table->opts.borders.no_outer;
    int no_inner_v      = table->opts.borders.no_inner_v;
    int rtl             = table->opts.rtl;
    int cpxl            = table->opts.cell_pad.left;
    int cpxr            = table->opts.cell_pad.right;
    int cpx             = cpxl + cpxr;
    size_t hcol_phys    = rtl ? (table_data->column_count - 1) : 0;

    const char *lc = tbc[bs].t_left;
    const char *rc = tbc[bs].t_right;
    if (is_rs && !no_outer) {
        size_t fc  = rtl ? (table_data->column_count - 1) : 0;
        size_t lcc = rtl ? 0 : (table_data->column_count - 1);
        if (is_rs[fc])  lc = tbc[bs].v;
        if (is_rs[lcc]) rc = tbc[bs].v;
    }
    tpre(table);
    if (!no_outer) print_ch(lc, oc);

    for (size_t ci = 0; ci < table_data->column_count; ci++) {
        size_t c = rtl ? (table_data->column_count - 1 - ci) : ci;

        if (is_rs && is_rs[c]) {
            /* Spanning column: try to render cell content at this line */
            ScColor col_bg = table_data->columns[c].opts.bg;
            if (rsc && rsc[c].cell) {
                int content_w = table->col_widths[c] - cpx;
                if (content_w < 0) content_w = 0;
                size_t ncl;
                TLine *cl = (table_data->columns[c].opts.word_wrap && content_w > 0)
                    ? wrap_cell_lines(rsc[c].cell, content_w, &ncl)
                    : make_cell_lines(rsc[c].cell, &ncl);
                int cn    = (int)ncl;
                int extra = rsc[c].span_h - cn;
                if (extra < 0) extra = 0;
                int top   = 0;
                if (rsc[c].valign == SC_VALIGN_MIDDLE) top = extra / 2;
                else if (rsc[c].valign == SC_VALIGN_BOTTOM) top = extra;
                int cli   = rsc[c].vis_offset - top;

                print_spaces_bg(cpxl, col_bg);
                if (cli >= 0 && cli < cn) {
                    TLine *line = &cl[cli];
                    int rw = content_w - (int)line->vis_w;
                    ScHAlign ha = table_data->columns[c].opts.halign;
                    if (rw >= 0) {
                        int lp = 0, rp = rw;
                        if (ha == SC_ALIGN_CENTER) { lp = rw/2; rp = rw-lp; }
                        else if (ha == SC_ALIGN_RIGHT) { lp = rw; rp = 0; }
                        print_spaces_bg(lp, col_bg);
                        for (size_t s = 0; s < line->count; s++)
                            print_span_bg(line->spans[s].text, line->spans[s].opts, col_bg);
                        print_spaces_bg(rp, col_bg);
                    } else {
                        int rem = content_w;
                        for (size_t s = 0; s < line->count && rem > 0; s++) {
                            const char *txt = line->spans[s].text;
                            int sw = (int)sc_utf8_string_length(txt, strlen(txt));
                            if (sw <= rem) {
                                print_span_bg(txt, line->spans[s].opts, col_bg);
                                rem -= sw;
                            } else {
                                size_t bytes = sc_utf8_trim_to_cols(txt, rem);
                                char *part = strndup(txt, bytes);
                                print_span_bg(part, line->spans[s].opts, col_bg);
                                free(part); rem = 0;
                            }
                        }
                    }
                } else {
                    print_spaces_bg(content_w, col_bg);
                }
                print_spaces_bg(cpxr, col_bg);
                free_tlines(cl, ncl);
            } else {
                print_spaces_bg(table->col_widths[c], col_bg);
            }
        } else {
            sc_apply_colors(ic, SC_ANSI_COLOR_NONE);
            for (int i = 0; i < table->col_widths[c]; i++) fputs(tbc[bs].h, stdout);
            fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
        }

        /* Junction between columns */
        if (ci < table_data->column_count - 1) {
            int is_hcol = (table->opts.header.col && c == hcol_phys);
            int has_vsep = !no_inner_v || is_hcol;
            if (has_vsep) {
                size_t nc   = rtl ? (c - 1) : (c + 1);
                int cur_rs  = is_rs ? is_rs[c]  : 0;
                int next_rs = is_rs ? (int)is_rs[nc] : 0;
                const char *jchar = tbc[bs].cross;
                if      (cur_rs && next_rs) jchar = tbc[bs].v;
                else if (cur_rs)            jchar = tbc[bs].t_left;
                else if (next_rs)           jchar = tbc[bs].t_right;
                ScColor jc = ic;
                if (is_hcol && table->opts.borders.header_col_sep_color.index != -2)
                    jc = table->opts.borders.header_col_sep_color;
                print_ch(jchar, jc);
            }
        }
    }

    if (!no_outer) print_ch(rc, oc);
    fputc('\n', stdout);
}

/* Title line (replaces top/bottom outer line when title is set) */
static void render_title_line(const Table *table, int is_top) {
    ScBorderType bs = table->opts.borders.style;
    ScColor oc = table->opts.borders.outer_color;
    const char *lc = is_top ? tbc[bs].tl : tbc[bs].bl;
    const char *rc = is_top ? tbc[bs].tr : tbc[bs].br;
    const char *h  = tbc[bs].h;
    int tpad = table->opts.title.pad > 0 ? table->opts.title.pad : 1;

    tpre(table);
    print_ch(lc, oc);
    if (table->opts.title.text && *table->opts.title.text) {
        int tlen   = (int)strlen(table->opts.title.text);
        int dashes = table->inner_w - tlen - 2 * tpad;
        if (dashes < 0) dashes = 0;
        int ld = 1, rd = dashes - 1;
        if (table->opts.title.align == SC_ALIGN_CENTER) { ld = dashes/2; rd = dashes - ld; }
        else if (table->opts.title.align == SC_ALIGN_RIGHT) { ld = dashes - 1; rd = 1; }
        if (ld < 0) ld = 0; if (rd < 0) rd = 0;
        sc_apply_colors(oc, SC_ANSI_COLOR_NONE);
        for (int i = 0; i < ld; i++) fputs(h, stdout);
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
        for (int i = 0; i < tpad; i++) print_ch(" ", oc);
        sc_print(table->opts.title.text, table->opts.title.style);
        for (int i = 0; i < tpad; i++) print_ch(" ", oc);
        sc_apply_colors(oc, SC_ANSI_COLOR_NONE);
        for (int i = 0; i < rd; i++) fputs(h, stdout);
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    } else {
        sc_apply_colors(oc, SC_ANSI_COLOR_NONE);
        for (int i = 0; i < table->inner_w; i++) fputs(h, stdout);
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    }
    print_ch(rc, oc);
    fputc('\n', stdout);
}

/* ── Row rendering ───────────────────────────────────────────────────────── */

/* row_kind: 0=data, 1=header, 2=footer
   row_h_in: pre-computed height (>0 for data rows); 0 = compute from cells
   rsc:      table->rsc for data rows (row_kind==0), NULL otherwise */
static void render_row(const Table *table, const ScCell *cells,
                        ScColor row_bg, int row_kind, int row_h_in) {
    const ScTableData *table_data = table->table_data;
    const RSCtx *rsc   = (row_kind == 0) ? table->rsc : NULL;
    ScBorderType bs  = table->opts.borders.style;
    ScColor oc       = table->opts.borders.outer_color;
    ScColor ic       = table->opts.borders.inner_color;
    int no_outer     = table->opts.borders.no_outer;
    int no_inner_v   = table->opts.borders.no_inner_v;
    int rtl          = table->opts.rtl;
    int cpx          = table->opts.cell_pad.left + table->opts.cell_pad.right;
    int cpxl         = table->opts.cell_pad.left;
    int cpxr         = table->opts.cell_pad.right;
    int cpy          = table->opts.cell_pad.top + table->opts.cell_pad.bottom;
    int cpt          = table->opts.cell_pad.top;
    int is_hdr       = (row_kind == 1);
    int is_ftr       = (row_kind == 2);

    /* physical header col index */
    size_t hcol_phys = rtl ? (table_data->column_count - 1) : 0;

    /* build cell lines (with wrap if enabled) */
    TLine **cl  = malloc(table_data->column_count * sizeof(TLine *));
    size_t *cnl = malloc(table_data->column_count * sizeof(size_t));
    size_t max_content = 0;

    /* Map logical render order → physical column index */
    for (size_t ci = 0; ci < table_data->column_count; ci++) {
        size_t c = rtl ? (table_data->column_count - 1 - ci) : ci;

        /* skip colspan-covered cells */
        int cs = cells[c].colspan;
        if (cs < 0) { cl[c] = NULL; cnl[c] = 0; continue; }

        /* rowspan continuation: build lines from spanning cell via RSCtx */
        if (cells[c].rowspan == -1) {
            if (rsc && rsc[c].cell) {
                int span_cw = table->col_widths[c] - cpx;
                if (span_cw < 0) span_cw = 0;
                if (table_data->columns[c].opts.word_wrap && span_cw > 0)
                    cl[c] = wrap_cell_lines(rsc[c].cell, span_cw, &cnl[c]);
                else
                    cl[c] = make_cell_lines(rsc[c].cell, &cnl[c]);
            } else {
                cl[c] = NULL; cnl[c] = 0;
            }
            continue; /* don't count towards row height */
        }

        /* effective colspan (clamped) */
        int ecs = (cs > 1) ? cs : 1;
        if ((int)c + ecs > (int)table_data->column_count) ecs = (int)table_data->column_count - (int)c;

        /* compute effective content width for this cell/span */
        int span_total = 0;
        for (int k = 0; k < ecs; k++) {
            size_t cc = c + (size_t)k;
            span_total += table->col_widths[cc];
            if (k < ecs - 1) {
                int is_hcol_k = (table->opts.header.col && cc == hcol_phys);
                if (!no_inner_v || is_hcol_k) span_total++; /* inner sep */
            }
        }
        int cw = span_total - cpx;
        if (cw < 0) cw = 0;

        if (table_data->columns[c].opts.word_wrap && cw > 0)
            cl[c] = wrap_cell_lines(&cells[c], cw, &cnl[c]);
        else
            cl[c] = make_cell_lines(&cells[c], &cnl[c]);

        /* rowspan starting cells don't determine individual row height */
        int rs_v = cells[c].rowspan;
        if ((rs_v == 0 || rs_v == 1) && cnl[c] > max_content)
            max_content = cnl[c];
    }

    int row_h = row_h_in > 0 ? row_h_in : (int)max_content + cpy;
    if (row_h < 1) row_h = 1;

    for (int li = 0; li < row_h; li++) {
        tpre(table);
        if (!no_outer) print_ch(tbc[bs].v, oc);

        size_t ci = 0;
        while (ci < table_data->column_count) {
            size_t c = rtl ? (table_data->column_count - 1 - ci) : ci;

            int cs_raw = cells[c].colspan;
            if (cs_raw < 0) { ci++; continue; } /* SKIP */

            int ecs = (cs_raw > 1) ? cs_raw : 1;
            if ((int)c + ecs > (int)table_data->column_count) ecs = (int)table_data->column_count - (int)c;

            /* per-cell background */
            ScColor col_bg  = table_data->columns[c].opts.bg;
            ScColor cell_bg = (row_bg.index != -2) ? row_bg : col_bg;
            int is_hcol_c   = (table->opts.header.col && c == hcol_phys);
            if (is_hcol_c && !is_hdr) {
                ScColor hcol_bg = is_ftr ? table->opts.footer.col_bg : table->opts.header.col_bg;
                cell_bg = hcol_bg;
            }
            if (is_hdr)              cell_bg = table->opts.header.row_bg;
            if (is_ftr && !is_hcol_c) cell_bg = table->opts.footer.row_bg;
            if (is_ftr &&  is_hcol_c) cell_bg = table->opts.footer.col_bg;

            /* alignment */
            ScHAlign ha = table_data->columns[c].opts.halign;
            ScVAlign va = table_data->columns[c].opts.valign;
            if (cells[c].align_set)  ha = cells[c].align;
            if (cells[c].valign_set) va = cells[c].valign;

            /* effective content width for this span */
            int span_total = 0;
            for (int k = 0; k < ecs; k++) {
                size_t cc = c + (size_t)k;
                span_total += table->col_widths[cc];
                if (k < ecs - 1) {
                    int is_hcol_k = (table->opts.header.col && cc == hcol_phys);
                    if (!no_inner_v || is_hcol_k) span_total++;
                }
            }
            int cw = span_total - cpx;
            if (cw < 0) cw = 0;

            /* vertical alignment */
            int cn = cl[c] ? (int)cnl[c] : 0;
            int cli;
            int rs_render = cells[c].rowspan;
            if ((rs_render > 1 || rs_render == -1) && rsc && rsc[c].cell) {
                /* span-aware: distribute content across full span height */
                int extra = rsc[c].span_h - cn;
                if (extra < 0) extra = 0;
                int top = 0;
                if (rsc[c].valign == SC_VALIGN_MIDDLE) top = extra / 2;
                else if (rsc[c].valign == SC_VALIGN_BOTTOM) top = extra;
                cli = rsc[c].vis_offset + li - top;
            } else {
                int extra = row_h - cn - cpy;
                if (extra < 0) extra = 0;
                int top_pad = cpt;
                if (va == SC_VALIGN_MIDDLE)      top_pad += extra / 2;
                else if (va == SC_VALIGN_BOTTOM) top_pad += extra;
                cli = li - top_pad;
            }

            print_spaces_bg(cpxl, cell_bg);

            if (cli >= 0 && cli < cn && cl[c]) {
                TLine *line = &cl[c][cli];
                int rw = cw - (int)line->vis_w;

                if (rw >= 0) {
                    int lp = 0, rp = rw;
                    if (ha == SC_ALIGN_CENTER) { lp = rw/2; rp = rw - lp; }
                    else if (ha == SC_ALIGN_RIGHT) { lp = rw; rp = 0; }
                    print_spaces_bg(lp, cell_bg);
                    for (size_t s = 0; s < line->count; s++) {
                        ScTextStyle so = line->spans[s].opts;
                        if (is_hdr) {
                            if (so.attr == 0) so.attr = table->opts.header.opts.attr;
                            if (so.fg.index == -2) so.fg = table->opts.header.opts.fg;
                        } else if (is_ftr) {
                            if (so.attr == 0) so.attr = table->opts.footer.opts.attr;
                            if (so.fg.index == -2) so.fg = table->opts.footer.opts.fg;
                        }
                        print_span_bg(line->spans[s].text, so, cell_bg);
                    }
                    print_spaces_bg(rp, cell_bg);
                } else {
                    /* content exceeds width: truncate (wrap already handled) */
                    int remaining = cw;
                    for (size_t s = 0; s < line->count && remaining > 0; s++) {
                        ScTextStyle so = line->spans[s].opts;
                        if (is_hdr) {
                            if (so.attr == 0) so.attr = table->opts.header.opts.attr;
                            if (so.fg.index == -2) so.fg = table->opts.header.opts.fg;
                        } else if (is_ftr) {
                            if (so.attr == 0) so.attr = table->opts.footer.opts.attr;
                            if (so.fg.index == -2) so.fg = table->opts.footer.opts.fg;
                        }
                        const char *txt = line->spans[s].text;
                        int sw = (int)sc_utf8_string_length(txt, strlen(txt));
                        if (sw <= remaining) {
                            print_span_bg(txt, so, cell_bg);
                            remaining -= sw;
                        } else {
                            size_t bytes = sc_utf8_trim_to_cols(txt, remaining);
                            char *part = strndup(txt, bytes);
                            print_span_bg(part, so, cell_bg);
                            free(part);
                            remaining = 0;
                        }
                    }
                }
            } else {
                print_spaces_bg(cw, cell_bg);
            }

            print_spaces_bg(cpxr, cell_bg);

            /* vertical separator after this cell/span */
            size_t end_col = c + (size_t)ecs - 1;
            if (ci + (size_t)ecs < table_data->column_count && bs != SC_BORDER_NONE) {
                int is_hcol_end = (table->opts.header.col && end_col == hcol_phys);
                if (!no_inner_v || is_hcol_end) {
                    ScColor sc_col = ic;
                    if (is_hcol_end
                            && table->opts.borders.header_col_sep_color.index != -2)
                        sc_col = table->opts.borders.header_col_sep_color;
                    print_ch(tbc[bs].v, sc_col);
                }
            }

            ci += (size_t)ecs;
        }

        if (!no_outer) print_ch(tbc[bs].v, oc);
        fputc('\n', stdout);
    }

    for (size_t c = 0; c < table_data->column_count; c++)
        if (cl[c]) free_tlines(cl[c], cnl[c]);
    free(cl);
    free(cnl);
}

static void render_top_border(const Table *table) {
    ScBorderType bs = table->opts.borders.style;
    ScColor oc = table->opts.borders.outer_color;
    if (table->opts.borders.no_outer) return;
    if (table->opts.title.text && table->opts.title.pos == SC_POSITION_TOP) {
        render_title_line(table, 1);
    } else if (bs != SC_BORDER_NONE) {
        render_horizontal_border(table, tbc[bs].tl, tbc[bs].tr,
                     tbc[bs].h, oc, tbc[bs].t_top, oc, oc, 0, NULL);
    }
}

static void render_header_row(const Table *table) {
    const ScTableData *table_data = table->table_data;
    if (!table->opts.header.row) return;
    ScBorderType bs  = table->opts.borders.style;
    ScColor oc       = table->opts.borders.outer_color;
    ScColor ic       = table->opts.borders.inner_color;
    ScColor hrsc     = table->opts.borders.header_row_sep_color;

    ScCell *hcells = malloc(table_data->column_count * sizeof(ScCell));
    for (size_t c = 0; c < table_data->column_count; c++) {
        hcells[c] = sc_cell(table_data->columns[c].header ? table_data->columns[c].header : "");
    }
    render_row(table, hcells, table->opts.header.row_bg, 1, 0);
    free(hcells);

    if (bs != SC_BORDER_NONE) {
        ScColor hc = (hrsc.index != -2) ? hrsc : ic;
        render_horizontal_border(table, tbc[bs].t_left, tbc[bs].t_right,
                     tbc[bs].h, hc, tbc[bs].cross, hc, oc, 1, NULL);
    }
}

static void render_data_rows(Table *table) {
    const ScTableData *table_data = table->table_data;
    ScBorderType bs = table->opts.borders.style;
    int no_inner_h  = table->opts.borders.no_inner_h;

    size_t max_r = table_data->row_count;
    if (table->opts.max_rows > 0 && (size_t)table->opts.max_rows < max_r)
        max_r = (size_t)table->opts.max_rows;

    for (size_t r = 0; r < max_r; r++) {
        for (size_t c = 0; c < table_data->column_count; c++) {
            int rs = table_data->rows[r].cells[c].rowspan;
            if (rs > 1) {
                int sep_h = (bs != SC_BORDER_NONE && !no_inner_h) ? 1 : 0;
                int sh = 0;
                for (int k = 0; k < rs && r + (size_t)k < table_data->row_count; k++) {
                    if (k > 0) sh += sep_h;
                    sh += table->row_heights[r + k];
                }
                ScVAlign va = table_data->columns[c].opts.valign;
                if (table_data->rows[r].cells[c].valign_set) va = table_data->rows[r].cells[c].valign;
                table->rsc[c] = (RSCtx){ &table_data->rows[r].cells[c], sh, 0, va };
            } else if (rs != -1) {
                table->rsc[c] = (RSCtx){ NULL, 0, 0, 0 };
            }
        }

        if (r > 0 && bs != SC_BORDER_NONE && !no_inner_h) {
            for (size_t c = 0; c < table_data->column_count; c++)
                table->is_rs[c] = (table_data->rows[r].cells[c].rowspan == -1);
            render_inner_sep(table);
            for (size_t c = 0; c < table_data->column_count; c++)
                if (table->is_rs[c] && table->rsc[c].cell) table->rsc[c].vis_offset++;
        }

        ScColor row_bg = table_data->rows[r].bg;
        if (row_bg.index == -2) {
            row_bg = SC_ANSI_COLOR_NONE;
            if (table->opts.striped && (r % 2 == 1)) row_bg = table->opts.stripe_bg;
        }
        render_row(table, table_data->rows[r].cells, row_bg, 0, table->row_heights[r]);

        for (size_t c = 0; c < table_data->column_count; c++)
            if (table->rsc[c].cell) table->rsc[c].vis_offset += table->row_heights[r];
    }

    if (table->opts.max_rows > 0 && table_data->row_count > max_r) {
        char msg[64];
        snprintf(msg, sizeof(msg), "… %zu more rows", table_data->row_count - max_r);
        ScTextStyle dim = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        ScCell *ind = malloc(table_data->column_count * sizeof(ScCell));
        ind[0] = sc_cell_cs(msg, (int)table_data->column_count);
        for (size_t c = 1; c < table_data->column_count; c++) ind[c] = sc_cell_skip();
        ScTextStyle saved = table->opts.header.opts;
        table->opts.header.opts = dim;
        render_row(table, ind, SC_ANSI_COLOR_NONE, 1, 0);
        table->opts.header.opts = saved;
        free(ind);
    }
}

static void render_footer_rows(const Table *table) {
    const ScTableData *table_data = table->table_data;
    ScBorderType bs  = table->opts.borders.style;
    ScColor oc       = table->opts.borders.outer_color;
    ScColor ic       = table->opts.borders.inner_color;
    ScColor hrsc     = table->opts.borders.header_row_sep_color;

    if (table_data->footer_row_count > 0 && bs != SC_BORDER_NONE) {
        ScColor hc = (hrsc.index != -2) ? hrsc : ic;
        render_horizontal_border(table, tbc[bs].t_left, tbc[bs].t_right,
                     tbc[bs].h, hc, tbc[bs].cross, hc, oc, 1, NULL);
    }
    for (size_t r = 0; r < table_data->footer_row_count; r++) {
        if (r > 0 && bs != SC_BORDER_NONE && !table->opts.borders.no_inner_h) {
            render_horizontal_border(table, tbc[bs].t_left, tbc[bs].t_right,
                         tbc[bs].h, ic, tbc[bs].cross, ic, oc, 1, NULL);
        }
        render_row(table, table_data->footer_rows[r].cells, table->opts.footer.row_bg, 2, 0);
    }
}

static void render_bottom_border(const Table *table) {
    ScBorderType bs = table->opts.borders.style;
    ScColor oc = table->opts.borders.outer_color;
    if (table->opts.borders.no_outer) return;
    if (table->opts.title.text && table->opts.title.pos == SC_POSITION_BOTTOM) {
        render_title_line(table, 0);
    } else if (bs != SC_BORDER_NONE) {
        render_horizontal_border(table, tbc[bs].bl, tbc[bs].br,
                     tbc[bs].h, oc, tbc[bs].t_bot, oc, oc, 0, NULL);
    }
}

