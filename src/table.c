#include "sparcli.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>

/* ── Internal line types (same concept as panel.c) ──────────────────────── */

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

static TLine *make_cell_lines(const ScCell *cell, size_t *out_n) {
    static const ScOptions PLAIN = { 0, { -2,0,0,0 }, { -2,0,0,0 } };

    size_t lines_cap = 4, nlines = 0;
    TLine *lines = malloc(lines_cap * sizeof(TLine));
    size_t buf_cap = 4, buf_n = 0, buf_w = 0;
    TSpan *buf = malloc(buf_cap * sizeof(TSpan));

    /* iterate (span_text, span_opts) pairs from cell */
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
                    if (buf_n == buf_cap) {
                        buf_cap *= 2;
                        buf = realloc(buf, buf_cap * sizeof(TSpan));
                    }
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
            if (buf_n == buf_cap) {
                buf_cap *= 2;
                buf = realloc(buf, buf_cap * sizeof(TSpan));
            }
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
    char     *header;   /* owned */
    ScColOpts opts;
    int       width;    /* computed: col content width + 2*cell_pad_x */
} TCol;

typedef struct {
    ScCell *cells;   /* owned array of ncols cells */
    size_t  ncols;
} TRow;

struct ScTable {
    TCol       *cols;
    size_t      ncols, cols_cap;
    TRow       *rows;
    size_t      nrows, rows_cap;
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
    if (bg.index != -2)
        sc_apply_colors(SC_COLOR_NONE, bg);
    for (int i = 0; i < n; i++) fputc(' ', stdout);
    if (bg.index != -2)
        fputs(SC_RESET, stdout);
}

static void print_span_bg(const char *text, ScOptions opts, ScColor cell_bg) {
    if (opts.bg.index == -2 && cell_bg.index != -2)
        opts.bg = cell_bg;
    sc_print(text, opts);
}

/* ── Column width computation ────────────────────────────────────────────── */

static void compute_col_widths(ScTable *t) {
    for (size_t c = 0; c < t->ncols; c++) {
        const char *hdr = t->cols[c].header;
        size_t max_w = hdr ? sc_utf8_vis_w(hdr, strlen(hdr)) : 0;
        for (size_t r = 0; r < t->nrows; r++) {
            size_t w = cell_vis_width(&t->rows[r].cells[c]);
            if (w > max_w) max_w = w;
        }

        int col_w = (int)max_w + 2 * t->opts.cell_pad_x;
        ScColOpts *co = &t->cols[c].opts;

        if (co->fixed_w > 0) {
            col_w = co->fixed_w;
        } else {
            if (co->min_w > 0 && col_w < co->min_w) col_w = co->min_w;
            if (co->max_w > 0 && col_w > co->max_w) col_w = co->max_w;
        }
        t->cols[c].width = col_w;
    }
}

static int table_inner_w(const ScTable *t) {
    int w = 0;
    for (size_t c = 0; c < t->ncols; c++) {
        w += t->cols[c].width;
        if (c < t->ncols - 1 && t->opts.borders.style != SC_BORDER_NONE) {
            int is_hcol = (t->opts.header_col && c == 0);
            if (!t->opts.borders.no_inner_v || is_hcol)
                w++;
        }
    }
    return w;
}

/* ── Horizontal rule rendering ───────────────────────────────────────────── */

/* edge_color:  color for lc/rc (outer chars like ╔╗╚╝╠╣)
   fill_color:  color for the fill segment (═══)
   mid_color:   color for junction chars (╬╦╩┼┬┴)
   use_hcol:    if 1, override mid_color at header-col boundary with
                header_col_sep_color
   lc/rc are skipped (not printed) when no_outer=1. */
static void render_hline(const ScTable *t, int *col_widths,
                          const char *lc, const char *rc,
                          const char *fill, ScColor fill_color,
                          const char *mid,  ScColor mid_color,
                          ScColor edge_color, int use_hcol) {
    if (!t->opts.borders.no_outer)
        print_ch(lc, edge_color);

    for (size_t c = 0; c < t->ncols; c++) {
        sc_apply_colors(fill_color, SC_COLOR_NONE);
        for (int i = 0; i < col_widths[c]; i++) fputs(fill, stdout);
        fputs(SC_RESET, stdout);

        if (c < t->ncols - 1 && mid) {
            int is_hcol = (t->opts.header_col && c == 0);
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

    if (!t->opts.borders.no_outer)
        print_ch(rc, edge_color);

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
        if (ld < 0) ld = 0;
        if (rd < 0) rd = 0;

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

static void render_row(const ScTable *t, const ScCell *cells,
                        ScColor row_bg, int is_hdr, int *col_widths) {
    ScBorderStyle bs = t->opts.borders.style;
    ScColor oc  = t->opts.borders.outer_color;
    ScColor ic  = t->opts.borders.inner_color;

    /* build cell lines for all columns */
    TLine **cl  = malloc(t->ncols * sizeof(TLine *));
    size_t *cnl = malloc(t->ncols * sizeof(size_t));
    size_t max_content = 0;

    for (size_t c = 0; c < t->ncols; c++) {
        cl[c] = make_cell_lines(&cells[c], &cnl[c]);
        if (cnl[c] > max_content) max_content = cnl[c];
    }

    int row_h = (int)max_content + 2 * t->opts.cell_pad_y;
    if (row_h < 1) row_h = 1;

    int no_outer  = t->opts.borders.no_outer;
    int no_inner_v = t->opts.borders.no_inner_v;

    for (int li = 0; li < row_h; li++) {
        /* left outer border */
        if (!no_outer)
            print_ch(tbc[bs].v, oc);

        for (size_t c = 0; c < t->ncols; c++) {
            /* per-cell background */
            ScColor cell_bg = row_bg;
            if (t->opts.header_col && c == 0 && !is_hdr)
                cell_bg = t->opts.header_col_bg;
            /* header row overrides header col */
            if (is_hdr) cell_bg = t->opts.header_row_bg;

            /* alignment */
            ScAlign  ha = t->cols[c].opts.align;
            ScValign va = t->cols[c].opts.valign;
            if (cells[c].align_set)  ha = cells[c].align;
            if (cells[c].valign_set) va = cells[c].valign;

            /* usable content width */
            int cpx  = t->opts.cell_pad_x;
            int cw   = col_widths[c] - 2 * cpx;
            if (cw < 0) cw = 0;

            /* vertical alignment: compute top_pad for this cell */
            int cn     = (int)cnl[c];
            int extra  = row_h - cn - 2 * t->opts.cell_pad_y;
            if (extra < 0) extra = 0;
            int top_pad = t->opts.cell_pad_y;
            if (va == SC_VALIGN_MIDDLE)      top_pad += extra / 2;
            else if (va == SC_VALIGN_BOTTOM) top_pad += extra;
            int cli = li - top_pad;

            print_spaces_bg(cpx, cell_bg);

            if (cli >= 0 && cli < cn) {
                TLine *line = &cl[c][cli];
                int rw = cw - (int)line->vis_w;

                if (rw >= 0) {
                    int lp = 0, rp = rw;
                    if (ha == SC_ALIGN_CENTER) { lp = rw / 2; rp = rw - lp; }
                    else if (ha == SC_ALIGN_RIGHT) { lp = rw; rp = 0; }

                    print_spaces_bg(lp, cell_bg);
                    for (size_t s = 0; s < line->count; s++) {
                        ScOptions so = line->spans[s].opts;
                        if (is_hdr) {
                            if (so.style == 0) so.style = t->opts.header_opts.style;
                            if (so.fg.index == -2) so.fg = t->opts.header_opts.fg;
                        }
                        print_span_bg(line->spans[s].text, so, cell_bg);
                    }
                    print_spaces_bg(rp, cell_bg);
                } else {
                    /* content exceeds column width: truncate to cw columns */
                    int remaining = cw;
                    for (size_t s = 0; s < line->count && remaining > 0; s++) {
                        ScOptions so = line->spans[s].opts;
                        if (is_hdr) {
                            if (so.style == 0) so.style = t->opts.header_opts.style;
                            if (so.fg.index == -2) so.fg = t->opts.header_opts.fg;
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

            /* column separator */
            if (c < t->ncols - 1 && bs != SC_BORDER_NONE) {
                int is_hcol = (t->opts.header_col && c == 0);
                if (!no_inner_v || is_hcol) {
                    ScColor sc = ic;
                    if (is_hcol
                            && t->opts.borders.header_col_sep_color.index != -2)
                        sc = t->opts.borders.header_col_sep_color;
                    print_ch(tbc[bs].v, sc);
                }
            }
        }

        /* right outer border */
        if (!no_outer)
            print_ch(tbc[bs].v, oc);
        fputc('\n', stdout);
    }

    for (size_t c = 0; c < t->ncols; c++) free_tlines(cl[c], cnl[c]);
    free(cl);
    free(cnl);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

ScTable *sc_table_new(ScTableOpts opts) {
    ScTable *t = malloc(sizeof(ScTable));
    t->cols = NULL; t->ncols = 0; t->cols_cap = 0;
    t->rows = NULL; t->nrows = 0; t->rows_cap = 0;
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

void sc_table_add_row(ScTable *t, ScCell *cells, size_t n) {
    if (t->nrows == t->rows_cap) {
        t->rows_cap = t->rows_cap ? t->rows_cap * 2 : 8;
        t->rows = realloc(t->rows, t->rows_cap * sizeof(TRow));
    }
    TRow *row = &t->rows[t->nrows++];
    row->ncols = t->ncols;
    row->cells = malloc(t->ncols * sizeof(ScCell));
    for (size_t i = 0; i < t->ncols; i++)
        row->cells[i] = (i < n) ? cells[i] : SC_CELL("");
}

void sc_table_free(ScTable *t) {
    for (size_t i = 0; i < t->ncols; i++) free(t->cols[i].header);
    free(t->cols);
    for (size_t i = 0; i < t->nrows; i++) free(t->rows[i].cells);
    free(t->rows);
    free(t);
}

void sc_table_print(const ScTable *t) {
    if (!t->ncols) return;

    /* cast away const for width computation (writes only .width) */
    compute_col_widths((ScTable *)t);

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

        /* header row separator */
        if (bs != SC_BORDER_NONE) {
            ScColor hc = (hrsc.index != -2) ? hrsc : ic;
            render_hline(t, cw, tbc[bs].t_left, tbc[bs].t_right,
                         tbc[bs].h, hc, tbc[bs].cross, hc, oc, 1);
        }
    }

    /* ── data rows ── */
    for (size_t r = 0; r < t->nrows; r++) {
        if (r > 0 && bs != SC_BORDER_NONE && !no_inner_h) {
            render_hline(t, cw, tbc[bs].t_left, tbc[bs].t_right,
                         tbc[bs].h, ic, tbc[bs].cross, ic, oc, 1);
        }

        ScColor row_bg = SC_COLOR_NONE;
        if (t->opts.striped && (r % 2 == 1)) row_bg = t->opts.stripe_bg;

        render_row(t, t->rows[r].cells, row_bg, 0, cw);
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
