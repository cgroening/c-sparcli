#include "sparcli.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Internal line types ─────────────────────────────────────────────────── */

typedef struct { const char *text; ScOptions opts; } TSpan;
typedef struct { TSpan *spans; size_t count; size_t vis_w; } TLine;

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
    static const ScOptions PLAIN = { 0, { -2,0,0,0 }, { -2,0,0,0 } };

    size_t lines_cap = 4, nlines = 0;
    TLine *lines = malloc(lines_cap * sizeof(TLine));
    size_t buf_cap = 4, buf_n = 0, buf_w = 0;
    TSpan *buf = malloc(buf_cap * sizeof(TSpan));

    size_t nspans = (cell->kind == SC_CELL_TEXT && cell->text) ? cell->text->count : 1;

    for (size_t si = 0; si < nspans; si++) {
        const char *s;
        ScOptions   opts;

        if (cell->kind == SC_CELL_STR) {
            s    = cell->str ? cell->str : "";
            opts = PLAIN;
        } else {
            s    = cell->text->spans[si].text;
            opts = cell->text->spans[si].opts;
        }

        const char *start = s;
        while (*s) {
            if (*s == '\n') {
                size_t len = (size_t)(s - start);
                if (len > 0) {
                    if (buf_n == buf_cap) { buf_cap *= 2; buf = realloc(buf, buf_cap * sizeof(TSpan)); }
                    buf[buf_n++] = (TSpan){ strndup(start, len), opts };
                    buf_w += sc_utf8_vis_w(start, len);
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
            buf_w += sc_utf8_vis_w(start, len);
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
    typedef struct { char *s; size_t vis_w; ScOptions opts; int is_space; } Tok;
    Tok *toks = NULL; size_t ntok = 0, tcap = 0;

    for (size_t si = 0; si < src->count; si++) {
        const char *p = src->spans[si].text;
        ScOptions opts = src->spans[si].opts;
        while (*p) {
            const char *seg = p;
            int is_sp = (*p == ' ');
            while (*p && ((*p == ' ') == is_sp)) p++;
            size_t len = (size_t)(p - seg);
            size_t vw  = is_sp ? len : sc_utf8_vis_w(seg, len);
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
                    size_t cw2  = sc_utf8_vis_w(p, cb);
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

/* ── Extended border character sets ─────────────────────────────────────── */

typedef struct {
    const char *tl, *tr, *bl, *br;
    const char *h, *v;
    const char *cross, *t_top, *t_bot, *t_left, *t_right;
} TBC;

static const TBC tbc[] = {
    [SC_BORDER_NONE]    = { " "," "," "," ", " "," ", " "," "," "," "," " },
    [SC_BORDER_ASCII]   = { "+","+","+","+", "-","|", "+","+","+","+","+" },
    [SC_BORDER_SINGLE]  = { "┌","┐","└","┘", "─","│", "┼","┬","┴","├","┤" },
    [SC_BORDER_DOUBLE]  = { "╔","╗","╚","╝", "═","║", "╬","╦","╩","╠","╣" },
    [SC_BORDER_ROUNDED] = { "╭","╮","╰","╯", "─","│", "┼","┬","┴","├","┤" },
    [SC_BORDER_THICK]   = { "┏","┓","┗","┛", "━","┃", "╋","┳","┻","┣","┫" },
};

/* ── Internal table structures ───────────────────────────────────────────── */

typedef struct {
    char     *header;
    ScColOpts opts;
    int       width;
} TCol;

typedef struct {
    ScCell *cells;
    size_t  ncols;
    ScColor bg;      /* SC_COLOR_NONE = use default / stripe */
} TRow;

struct ScTable {
    TCol       *cols;
    size_t      ncols, cols_cap;
    TRow       *rows;
    size_t      nrows, rows_cap;
    TRow       *footer_rows;
    size_t      nfooter_rows, footer_rows_cap;
    ScTableOpts opts;
};

/* ── Low-level print helpers ─────────────────────────────────────────────── */

static void print_ch(const char *s, ScColor fg) {
    sc_apply_colors(fg, SC_COLOR_NONE);
    fputs(s, stdout);
    fputs(SC_RESET, stdout);
}

static void print_spaces_bg(int n, ScColor bg) {
    if (n <= 0) return;
    if (bg.index != -2) sc_apply_colors(SC_COLOR_NONE, bg);
    for (int i = 0; i < n; i++) fputc(' ', stdout);
    if (bg.index != -2) fputs(SC_RESET, stdout);
}

static void print_span_bg(const char *text, ScOptions opts, ScColor cell_bg) {
    if (opts.bg.index == -2 && cell_bg.index != -2) opts.bg = cell_bg;
    sc_print(text, opts);
}

/* ── Column width computation ────────────────────────────────────────────── */

static void compute_col_widths(ScTable *t) {
    for (size_t c = 0; c < t->ncols; c++) {
        const char *hdr = t->cols[c].header;
        size_t max_w = hdr ? sc_utf8_vis_w(hdr, strlen(hdr)) : 0;

        /* only consider cells with colspan <= 1 (not spanning cells) */
        for (size_t r = 0; r < t->nrows; r++) {
            if (c >= t->rows[r].ncols) continue;
            ScCell *cell = &t->rows[r].cells[c];
            if (cell->colspan != 0 && cell->colspan != 1) continue; /* skip/span */
            size_t w = cell_vis_width(cell);
            if (w > max_w) max_w = w;
        }
        for (size_t r = 0; r < t->nfooter_rows; r++) {
            if (c >= t->footer_rows[r].ncols) continue;
            ScCell *cell = &t->footer_rows[r].cells[c];
            if (cell->colspan != 0 && cell->colspan != 1) continue;
            size_t w = cell_vis_width(cell);
            if (w > max_w) max_w = w;
        }

        int col_w = (int)max_w + 2 * t->opts.cell_pad_x;
        ScColOpts *co = &t->cols[c].opts;
        if (co->fixed_w > 0)       col_w = co->fixed_w;
        else {
            if (co->min_w > 0 && col_w < co->min_w) col_w = co->min_w;
            if (co->max_w > 0 && col_w > co->max_w) col_w = co->max_w;
        }
        t->cols[c].width = col_w;
    }
}

/* Scale flex columns to reach total_width. */
static void apply_total_width(ScTable *t) {
    if (t->opts.total_width <= 0) return;
    ScBorderStyle bs = t->opts.borders.style;
    int no_outer = t->opts.borders.no_outer;

    int outer = (bs != SC_BORDER_NONE && !no_outer) ? 2 : 0;
    int seps = 0;
    for (size_t c = 0; c + 1 < t->ncols; c++) {
        int is_hcol = (t->opts.header_col && c == 0);
        if (bs != SC_BORDER_NONE && (!t->opts.borders.no_inner_v || is_hcol))
            seps++;
    }
    int current = outer + seps;
    for (size_t c = 0; c < t->ncols; c++) current += t->cols[c].width;

    int delta = t->opts.total_width - current;
    if (delta == 0) return;

    /* count flex columns */
    int nflex = 0;
    for (size_t c = 0; c < t->ncols; c++)
        if (t->cols[c].opts.fixed_w == 0) nflex++;
    if (nflex == 0) return;

    int per = delta / nflex;
    int rem = delta - per * nflex;
    int sign = (rem >= 0) ? 1 : -1;
    rem = (rem < 0) ? -rem : rem;

    for (size_t c = 0; c < t->ncols; c++) {
        if (t->cols[c].opts.fixed_w > 0) continue;
        int adj = per + (rem > 0 ? sign : 0);
        if (rem > 0) rem--;
        int new_w = t->cols[c].width + adj;
        if (new_w < 2) new_w = 2;
        t->cols[c].width = new_w;
    }
}

static int table_inner_w(const ScTable *t) {
    int w = 0;
    for (size_t c = 0; c < t->ncols; c++) {
        w += t->cols[c].width;
        if (c < t->ncols - 1 && t->opts.borders.style != SC_BORDER_NONE) {
            int is_hcol = (t->opts.header_col && c == 0);
            if (!t->opts.borders.no_inner_v || is_hcol) w++;
        }
    }
    return w;
}

/* ── Horizontal rule rendering ───────────────────────────────────────────── */

static void render_hline(const ScTable *t, int *col_widths,
                          const char *lc, const char *rc,
                          const char *fill, ScColor fill_color,
                          const char *mid,  ScColor mid_color,
                          ScColor edge_color, int use_hcol) {
    int rtl = t->opts.rtl;
    int no_outer = t->opts.borders.no_outer;
    if (!no_outer) print_ch(lc, edge_color);

    for (size_t ci = 0; ci < t->ncols; ci++) {
        size_t c = rtl ? (t->ncols - 1 - ci) : ci;
        sc_apply_colors(fill_color, SC_COLOR_NONE);
        for (int i = 0; i < col_widths[c]; i++) fputs(fill, stdout);
        fputs(SC_RESET, stdout);

        if (ci < t->ncols - 1 && mid) {
            /* physical col for hcol check: col 0 in LTR, col ncols-1 in RTL */
            size_t hcol_phys = rtl ? (t->ncols - 1) : 0;
            int is_hcol = (t->opts.header_col && c == hcol_phys);
            int has_vsep = !t->opts.borders.no_inner_v || is_hcol;
            if (has_vsep) {
                ScColor jc = mid_color;
                if (use_hcol && is_hcol
                        && t->opts.borders.header_col_sep_color.index != -2)
                    jc = t->opts.borders.header_col_sep_color;
                print_ch(mid, jc);
            }
        }
    }

    if (!no_outer) print_ch(rc, edge_color);
    fputc('\n', stdout);
}

/* Title line (replaces top/bottom outer line when title is set) */
static void render_title_line(const ScTable *t, int inner_w, int is_top) {
    ScBorderStyle bs = t->opts.borders.style;
    ScColor oc = t->opts.borders.outer_color;
    const char *lc = is_top ? tbc[bs].tl : tbc[bs].bl;
    const char *rc = is_top ? tbc[bs].tr : tbc[bs].br;
    const char *h  = tbc[bs].h;
    int tpad = t->opts.title_pad > 0 ? t->opts.title_pad : 1;

    print_ch(lc, oc);
    if (t->opts.title && *t->opts.title) {
        int tlen   = (int)strlen(t->opts.title);
        int dashes = inner_w - tlen - 2 * tpad;
        if (dashes < 0) dashes = 0;
        int ld = 1, rd = dashes - 1;
        if (t->opts.title_align == SC_ALIGN_CENTER) { ld = dashes/2; rd = dashes - ld; }
        else if (t->opts.title_align == SC_ALIGN_RIGHT) { ld = dashes - 1; rd = 1; }
        if (ld < 0) ld = 0; if (rd < 0) rd = 0;
        sc_apply_colors(oc, SC_COLOR_NONE);
        for (int i = 0; i < ld; i++) fputs(h, stdout);
        fputs(SC_RESET, stdout);
        for (int i = 0; i < tpad; i++) print_ch(" ", oc);
        sc_print(t->opts.title, t->opts.title_opts);
        for (int i = 0; i < tpad; i++) print_ch(" ", oc);
        sc_apply_colors(oc, SC_COLOR_NONE);
        for (int i = 0; i < rd; i++) fputs(h, stdout);
        fputs(SC_RESET, stdout);
    } else {
        sc_apply_colors(oc, SC_COLOR_NONE);
        for (int i = 0; i < inner_w; i++) fputs(h, stdout);
        fputs(SC_RESET, stdout);
    }
    print_ch(rc, oc);
    fputc('\n', stdout);
}

/* ── Row rendering ───────────────────────────────────────────────────────── */

/* row_kind: 0=data, 1=header, 2=footer */
static void render_row(const ScTable *t, const ScCell *cells,
                        ScColor row_bg, int row_kind, int *col_widths) {
    ScBorderStyle bs  = t->opts.borders.style;
    ScColor oc  = t->opts.borders.outer_color;
    ScColor ic  = t->opts.borders.inner_color;
    int no_outer   = t->opts.borders.no_outer;
    int no_inner_v = t->opts.borders.no_inner_v;
    int rtl        = t->opts.rtl;
    int cpx        = t->opts.cell_pad_x;
    int cpy        = t->opts.cell_pad_y;
    int is_hdr     = (row_kind == 1);
    int is_ftr     = (row_kind == 2);

    /* physical header col index */
    size_t hcol_phys = rtl ? (t->ncols - 1) : 0;

    /* build cell lines (with wrap if enabled) */
    TLine **cl  = malloc(t->ncols * sizeof(TLine *));
    size_t *cnl = malloc(t->ncols * sizeof(size_t));
    size_t max_content = 0;

    /* Map logical render order → physical column index */
    for (size_t ci = 0; ci < t->ncols; ci++) {
        size_t c = rtl ? (t->ncols - 1 - ci) : ci;

        /* skip SKIP cells and continuation cells */
        int cs = cells[c].colspan;
        if (cs < 0) { cl[c] = NULL; cnl[c] = 0; continue; }

        /* effective colspan (clamped) */
        int ecs = (cs > 1) ? cs : 1;
        if ((int)c + ecs > (int)t->ncols) ecs = (int)t->ncols - (int)c;

        /* compute effective content width for this cell/span */
        int span_total = 0;
        for (int k = 0; k < ecs; k++) {
            size_t cc = c + (size_t)k;
            span_total += col_widths[cc];
            if (k < ecs - 1) {
                int is_hcol_k = (t->opts.header_col && cc == hcol_phys);
                if (!no_inner_v || is_hcol_k) span_total++; /* inner sep */
            }
        }
        int cw = span_total - 2 * cpx;
        if (cw < 0) cw = 0;

        if (t->cols[c].opts.wrap && cw > 0)
            cl[c] = wrap_cell_lines(&cells[c], cw, &cnl[c]);
        else
            cl[c] = make_cell_lines(&cells[c], &cnl[c]);

        if (cnl[c] > max_content) max_content = cnl[c];
    }

    int row_h = (int)max_content + 2 * cpy;
    if (row_h < 1) row_h = 1;

    for (int li = 0; li < row_h; li++) {
        if (!no_outer) print_ch(tbc[bs].v, oc);

        size_t ci = 0;
        while (ci < t->ncols) {
            size_t c = rtl ? (t->ncols - 1 - ci) : ci;

            int cs_raw = cells[c].colspan;
            if (cs_raw < 0) { ci++; continue; } /* SKIP */

            int ecs = (cs_raw > 1) ? cs_raw : 1;
            if ((int)c + ecs > (int)t->ncols) ecs = (int)t->ncols - (int)c;

            /* per-cell background */
            ScColor cell_bg = row_bg;
            int is_hcol_c = (t->opts.header_col && c == hcol_phys);
            if (is_hcol_c && !is_hdr) {
                ScColor hcol_bg = is_ftr ? t->opts.footer_col_bg : t->opts.header_col_bg;
                cell_bg = hcol_bg;
            }
            if (is_hdr)  cell_bg = t->opts.header_row_bg;
            if (is_ftr && !is_hcol_c) cell_bg = t->opts.footer_row_bg;
            if (is_ftr &&  is_hcol_c) cell_bg = t->opts.footer_col_bg;

            /* alignment */
            ScAlign  ha = t->cols[c].opts.align;
            ScValign va = t->cols[c].opts.valign;
            if (cells[c].align_set)  ha = cells[c].align;
            if (cells[c].valign_set) va = cells[c].valign;

            /* effective content width for this span */
            int span_total = 0;
            for (int k = 0; k < ecs; k++) {
                size_t cc = c + (size_t)k;
                span_total += col_widths[cc];
                if (k < ecs - 1) {
                    int is_hcol_k = (t->opts.header_col && cc == hcol_phys);
                    if (!no_inner_v || is_hcol_k) span_total++;
                }
            }
            int cw = span_total - 2 * cpx;
            if (cw < 0) cw = 0;

            /* vertical alignment */
            int cn     = cl[c] ? (int)cnl[c] : 0;
            int extra  = row_h - cn - 2 * cpy;
            if (extra < 0) extra = 0;
            int top_pad = cpy;
            if (va == SC_VALIGN_MIDDLE)      top_pad += extra / 2;
            else if (va == SC_VALIGN_BOTTOM) top_pad += extra;
            int cli = li - top_pad;

            print_spaces_bg(cpx, cell_bg);

            if (cli >= 0 && cli < cn && cl[c]) {
                TLine *line = &cl[c][cli];
                int rw = cw - (int)line->vis_w;

                if (rw >= 0) {
                    int lp = 0, rp = rw;
                    if (ha == SC_ALIGN_CENTER) { lp = rw/2; rp = rw - lp; }
                    else if (ha == SC_ALIGN_RIGHT) { lp = rw; rp = 0; }
                    print_spaces_bg(lp, cell_bg);
                    for (size_t s = 0; s < line->count; s++) {
                        ScOptions so = line->spans[s].opts;
                        if (is_hdr) {
                            if (so.style == 0) so.style = t->opts.header_opts.style;
                            if (so.fg.index == -2) so.fg = t->opts.header_opts.fg;
                        } else if (is_ftr) {
                            if (so.style == 0) so.style = t->opts.footer_opts.style;
                            if (so.fg.index == -2) so.fg = t->opts.footer_opts.fg;
                        }
                        print_span_bg(line->spans[s].text, so, cell_bg);
                    }
                    print_spaces_bg(rp, cell_bg);
                } else {
                    /* content exceeds width: truncate (wrap already handled) */
                    int remaining = cw;
                    for (size_t s = 0; s < line->count && remaining > 0; s++) {
                        ScOptions so = line->spans[s].opts;
                        if (is_hdr) {
                            if (so.style == 0) so.style = t->opts.header_opts.style;
                            if (so.fg.index == -2) so.fg = t->opts.header_opts.fg;
                        } else if (is_ftr) {
                            if (so.style == 0) so.style = t->opts.footer_opts.style;
                            if (so.fg.index == -2) so.fg = t->opts.footer_opts.fg;
                        }
                        const char *txt = line->spans[s].text;
                        int sw = (int)sc_utf8_vis_w(txt, strlen(txt));
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

            print_spaces_bg(cpx, cell_bg);

            /* vertical separator after this cell/span */
            size_t end_col = c + (size_t)ecs - 1;
            if (ci + (size_t)ecs < t->ncols && bs != SC_BORDER_NONE) {
                int is_hcol_end = (t->opts.header_col && end_col == hcol_phys);
                if (!no_inner_v || is_hcol_end) {
                    ScColor sc_col = ic;
                    if (is_hcol_end
                            && t->opts.borders.header_col_sep_color.index != -2)
                        sc_col = t->opts.borders.header_col_sep_color;
                    print_ch(tbc[bs].v, sc_col);
                }
            }

            ci += (size_t)ecs;
        }

        if (!no_outer) print_ch(tbc[bs].v, oc);
        fputc('\n', stdout);
    }

    for (size_t c = 0; c < t->ncols; c++)
        if (cl[c]) free_tlines(cl[c], cnl[c]);
    free(cl);
    free(cnl);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

ScTable *sc_table_new(ScTableOpts opts) {
    ScTable *t = malloc(sizeof(ScTable));
    t->cols = NULL; t->ncols = 0; t->cols_cap = 0;
    t->rows = NULL; t->nrows = 0; t->rows_cap = 0;
    t->footer_rows = NULL; t->nfooter_rows = 0; t->footer_rows_cap = 0;
    t->opts = opts;
    return t;
}

void sc_table_add_col(ScTable *t, const char *header, ScColOpts col) {
    if (t->ncols == t->cols_cap) {
        t->cols_cap = t->cols_cap ? t->cols_cap * 2 : 4;
        t->cols = realloc(t->cols, t->cols_cap * sizeof(TCol));
    }
    t->cols[t->ncols].header = header ? strdup(header) : NULL;
    t->cols[t->ncols].opts   = col;
    t->cols[t->ncols].width  = 0;
    t->ncols++;
}

static void add_row_to(TRow **arr, size_t *n, size_t *cap,
                        ScTable *t, ScCell *cells, size_t ncell, ScColor bg) {
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 8;
        *arr = realloc(*arr, *cap * sizeof(TRow));
    }
    TRow *row = &(*arr)[(*n)++];
    row->ncols = t->ncols;
    row->bg    = bg;
    row->cells = malloc(t->ncols * sizeof(ScCell));
    for (size_t i = 0; i < t->ncols; i++)
        row->cells[i] = (i < ncell) ? cells[i] : SC_CELL("");
}

void sc_table_add_row(ScTable *t, ScCell *cells, size_t n) {
    add_row_to(&t->rows, &t->nrows, &t->rows_cap, t, cells, n, SC_COLOR_NONE);
}

void sc_table_add_row_bg(ScTable *t, ScCell *cells, size_t n, ScColor bg) {
    add_row_to(&t->rows, &t->nrows, &t->rows_cap, t, cells, n, bg);
}

void sc_table_add_footer_row(ScTable *t, ScCell *cells, size_t n) {
    add_row_to(&t->footer_rows, &t->nfooter_rows, &t->footer_rows_cap,
               t, cells, n, SC_COLOR_NONE);
}

void sc_table_free(ScTable *t) {
    for (size_t i = 0; i < t->ncols; i++) free(t->cols[i].header);
    free(t->cols);
    for (size_t i = 0; i < t->nrows; i++) free(t->rows[i].cells);
    free(t->rows);
    for (size_t i = 0; i < t->nfooter_rows; i++) free(t->footer_rows[i].cells);
    free(t->footer_rows);
    free(t);
}

void sc_table_print(const ScTable *t) {
    if (!t->ncols) return;

    compute_col_widths((ScTable *)t);
    apply_total_width((ScTable *)t);

    int *cw = malloc(t->ncols * sizeof(int));
    for (size_t c = 0; c < t->ncols; c++) cw[c] = t->cols[c].width;

    ScBorderStyle bs    = t->opts.borders.style;
    ScColor oc    = t->opts.borders.outer_color;
    ScColor ic    = t->opts.borders.inner_color;
    ScColor hrsc  = t->opts.borders.header_row_sep_color;
    int no_outer  = t->opts.borders.no_outer;
    int no_inner_h = t->opts.borders.no_inner_h;

    int inner_w = table_inner_w(t);

    /* ── top border ── */
    if (!no_outer) {
        if (t->opts.title && t->opts.title_pos == SC_TITLE_TOP) {
            render_title_line(t, inner_w, 1);
        } else if (bs != SC_BORDER_NONE) {
            render_hline(t, cw, tbc[bs].tl, tbc[bs].tr,
                         tbc[bs].h, oc, tbc[bs].t_top, oc, oc, 0);
        }
    }

    /* ── header row ── */
    if (t->opts.header_row) {
        ScCell *hcells = malloc(t->ncols * sizeof(ScCell));
        for (size_t c = 0; c < t->ncols; c++)
            hcells[c] = SC_CELL(t->cols[c].header ? t->cols[c].header : "");
        render_row(t, hcells, t->opts.header_row_bg, 1, cw);
        free(hcells);

        if (bs != SC_BORDER_NONE) {
            ScColor hc = (hrsc.index != -2) ? hrsc : ic;
            render_hline(t, cw, tbc[bs].t_left, tbc[bs].t_right,
                         tbc[bs].h, hc, tbc[bs].cross, hc, oc, 1);
        }
    }

    /* ── data rows ── */
    size_t max_r = t->nrows;
    if (t->opts.max_rows > 0 && (size_t)t->opts.max_rows < max_r)
        max_r = (size_t)t->opts.max_rows;

    for (size_t r = 0; r < max_r; r++) {
        if (r > 0 && bs != SC_BORDER_NONE && !no_inner_h) {
            render_hline(t, cw, tbc[bs].t_left, tbc[bs].t_right,
                         tbc[bs].h, ic, tbc[bs].cross, ic, oc, 1);
        }
        ScColor row_bg = t->rows[r].bg;
        if (row_bg.index == -2) {
            row_bg = SC_COLOR_NONE;
            if (t->opts.striped && (r % 2 == 1)) row_bg = t->opts.stripe_bg;
        }
        render_row(t, t->rows[r].cells, row_bg, 0, cw);
    }

    /* ── truncation indicator ── */
    if (t->opts.max_rows > 0 && t->nrows > max_r) {
        char msg[64];
        snprintf(msg, sizeof(msg), "… %zu more rows", t->nrows - max_r);
        ScOptions dim = { SC_STYLE_DIM, SC_COLOR_NONE, SC_COLOR_NONE };
        ScCell *ind = malloc(t->ncols * sizeof(ScCell));
        ind[0] = SC_CELL_CS(msg, (int)t->ncols);
        for (size_t c = 1; c < t->ncols; c++) ind[c] = SC_CELL_SKIP;

        /* temporarily set opts for the indicator: use dim style */
        ScTable *mt = (ScTable *)t;
        ScOptions saved_hopts = mt->opts.header_opts;
        mt->opts.header_opts = dim;
        render_row(t, ind, SC_COLOR_NONE, 1, cw);
        mt->opts.header_opts = saved_hopts;
        free(ind);
    }

    /* ── footer rows ── */
    if (t->nfooter_rows > 0 && bs != SC_BORDER_NONE) {
        ScColor hc = (hrsc.index != -2) ? hrsc : ic;
        render_hline(t, cw, tbc[bs].t_left, tbc[bs].t_right,
                     tbc[bs].h, hc, tbc[bs].cross, hc, oc, 1);
    }
    for (size_t r = 0; r < t->nfooter_rows; r++) {
        if (r > 0 && bs != SC_BORDER_NONE && !no_inner_h) {
            render_hline(t, cw, tbc[bs].t_left, tbc[bs].t_right,
                         tbc[bs].h, ic, tbc[bs].cross, ic, oc, 1);
        }
        render_row(t, t->footer_rows[r].cells, t->opts.footer_row_bg, 2, cw);
    }

    /* ── bottom border ── */
    if (!no_outer) {
        if (t->opts.title && t->opts.title_pos == SC_TITLE_BOTTOM) {
            render_title_line(t, inner_w, 0);
        } else if (bs != SC_BORDER_NONE) {
            render_hline(t, cw, tbc[bs].bl, tbc[bs].br,
                         tbc[bs].h, oc, tbc[bs].t_bot, oc, oc, 0);
        }
    }

    free(cw);
}
