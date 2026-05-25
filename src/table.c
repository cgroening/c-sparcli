#include "sparcli.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


typedef struct {
    const char *text;
    ScTextStyle opts; } TSpan;

typedef struct {
    TSpan *spans;
    size_t count;
    size_t vis_w;
} TLine;

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

/* ── Rowspan context (tracks active spans across rows) ───────────────────── */

typedef struct {
    const ScCell *cell;
    int           span_h;
    int           vis_offset;
    ScVAlign      valign;
} RSCtx;

/* ── Internal table structures ───────────────────────────────────────────── */

typedef struct {
    char     *header;
    ScColOpts opts;
    int       width;
} TCol;

typedef struct {
    ScCell *cells;
    size_t  ncols;
    ScColor bg;
} TRow;

struct ScTable {
    ScTableOpts opts;
    TCol       *columns;
    size_t      column_capacity;
    size_t      column_count;
    TRow       *rows;
    size_t      row_capacity;
    size_t      row_count;
    TRow       *footer_rows;
    size_t      footer_rows_capacity;
    size_t      footer_rows_count;
};

/* ── Forward declarations ────────────────────────────────────────────────── */

ScTable *sc_table_new(ScTableOpts opts);

void sc_table_add_column(ScTable *table, const char *header, ScColOpts col);
    static void *dynarray_grow(
        void *array,
        size_t *current_capacity,
        size_t element_size,
        size_t initial_capacity
    );

void sc_table_add_row(ScTable *table, ScCell *cells, size_t n);
void sc_table_add_row_bg(ScTable *table, ScCell *cells, size_t n, ScColor bg);
void sc_table_add_footer_row(ScTable *table, ScCell *cells, size_t n);
    static void add_row_to(
        ScTable *t, ScCell *cells, size_t ncell, ScColor bg, bool to_footer
    );

void sc_table_print(const ScTable *table);

void sc_table_free(ScTable *table);
    static void free_row(TRow *row);
    static void free_row_array(TRow *rows, size_t count);







static void free_tlines(TLine *lines, size_t n);
static void flush_tline(TLine **lines, size_t *cap, size_t *n,
                            TSpan *buf, size_t buf_n, size_t vis_w);
static TLine *make_cell_lines(const ScCell *cell, size_t *out_n);
static size_t cell_vis_width(const ScCell *cell);
static TLine *wrap_one_tline(const TLine *src, int wrap_w, size_t *out_n);
static TLine *wrap_cell_lines(const ScCell *cell, int wrap_w, size_t *out_n);
static void print_ch(const char *s, ScColor fg);
static void print_spaces_bg(int n, ScColor bg);
static void print_span_bg(const char *text, ScTextStyle opts, ScColor cell_bg);
static void compute_col_widths(ScTable *t);
static void apply_total_width(ScTable *t);
static int table_inner_w(const ScTable *t);
static int *compute_row_heights(const ScTable *t, const int *cw);
static void tpre(const ScTable *t);
static void render_horizontal_border(const ScTable *t, int *col_widths,
                                         const char *lc, const char *rc,
                                         const char *fill, ScColor fill_color,
                                         const char *mid,  ScColor mid_color,
                                         ScColor edge_color, int use_hcol,
                                         const int *rs);
static void    render_inner_sep(const ScTable *t, int *cw,
                                 ScColor ic, const int *is_rs, const RSCtx *rsc);
static void    render_title_line(const ScTable *t, int inner_w, int is_top);
static void    render_row(const ScTable *t, const ScCell *cells,
                           ScColor row_bg, int row_kind, int *col_widths,
                           int row_h_in, const RSCtx *rsc);




ScTable *sc_table_new(ScTableOpts opts) {
    ScTable *table = malloc(sizeof(ScTable));
    table->columns = NULL;
    table->column_capacity = 0;
    table->column_count = 0;
    table->rows = NULL;
    table->row_capacity = 0;
    table->row_count = 0;
    table->footer_rows = NULL;
    table->footer_rows_capacity = 0;
    table->footer_rows_count = 0;
    table->opts = opts;
    return table;
}

void sc_table_add_column(
    ScTable *table, const char *header, ScColOpts col
) {
    // Ensure there is enough capacity for the new column, growing if necessary
    if (table->column_count == table->column_capacity) {
        table->columns = dynarray_grow(
            table->columns, &table->column_capacity, sizeof(TCol), 4
        );
    }

    // Add the new column with the provided header and options
    table->columns[table->column_count].header = header ? strdup(header) : NULL;
    table->columns[table->column_count].opts   = col;
    table->columns[table->column_count].width  = 0;
    table->column_count++;
}


/**
 * @brief Grows a dynamic array by doubling its capacity.
 *
 * @param array            Pointer to the existing array or `NULL`.
 * @param current_capacity Current allocated capacity; updated in place.
 * @param element_size     Size of a single element in bytes.
 * @param initial_capacity Capacity to allocate when the array is empty.
 * @return                 Pointer to the reallocated array.
 */
static void *dynarray_grow(
    void *array,
    size_t *current_capacity,
    size_t element_size,
    size_t initial_capacity
) {
    // Double the capacity or set to initial_capacity if currently zero.
    if (*current_capacity == 0) {
        *current_capacity = initial_capacity;
    }
    else {
        *current_capacity *= 2;
    }

    // Reallocate the array to the new capacity, abort on failure
    void *tmp = realloc(array, *current_capacity * element_size);
    if (!tmp) {
        abort();
    }
    return tmp;
}


void sc_table_add_row(ScTable *table, ScCell *cells, size_t n) {
    add_row_to(table, cells, n, SC_ANSI_COLOR_NONE, false);
}

void sc_table_add_row_bg(ScTable *table, ScCell *cells, size_t n, ScColor bg) {
    add_row_to(table, cells, n, bg, false);
}

void sc_table_add_footer_row(ScTable *table, ScCell *cells, size_t n) {
    add_row_to(table, cells, n, SC_ANSI_COLOR_NONE, true);
}

/**
 *
 */
static void add_row_to(
    ScTable *t, ScCell *cells, size_t ncell, ScColor bg, bool to_footer
) {
    // Determine which array and counters to use based on the target (main rows
    // or footer rows)
    TRow **row_array = to_footer ? &t->footer_rows : &t->rows;
    size_t *row_count = to_footer ? &t->footer_rows_count : &t->row_count;
    size_t *capacity = to_footer ? &t->footer_rows_capacity : &t->row_capacity;

    // Grow the array if capacity is reached
    if (*row_count == *capacity) {
        *row_array = dynarray_grow(*row_array, capacity, sizeof(TRow), 8);
    }

    // Determin address of the new row and initialize it
    TRow *row = &(*row_array)[(*row_count)++];
    row->ncols = t->column_count;
    row->bg    = bg;
    row->cells = malloc(t->column_count * sizeof(ScCell));

    // Copy provided cells into the new row, filling remaining cells with
    // empty content
    for (size_t i = 0; i < t->column_count; i++)
        row->cells[i] = (i < ncell) ? cells[i] : sc_cell("");
}











void sc_table_print(const ScTable *table) {
    if (!table->column_count) return;

    for (int i = 0; i < table->opts.margin.top; i++) fputc('\n', stdout);
    compute_col_widths((ScTable *)table);
    apply_total_width((ScTable *)table);

    int *cw = malloc(table->column_count * sizeof(int));
    for (size_t c = 0; c < table->column_count; c++) cw[c] = table->columns[c].width;

    int *row_h_arr = compute_row_heights(table, cw);
    RSCtx *rsc = calloc(table->column_count, sizeof(RSCtx));

    ScBorderType bs    = table->opts.borders.style;
    ScColor oc    = table->opts.borders.outer_color;
    ScColor ic    = table->opts.borders.inner_color;
    ScColor hrsc  = table->opts.borders.header_row_sep_color;
    int no_outer  = table->opts.borders.no_outer;
    int no_inner_h = table->opts.borders.no_inner_h;

    int inner_w = table_inner_w(table);

    int *is_rs = calloc(table->column_count, sizeof(int));

    /* ── top border ── */
    if (!no_outer) {
        if (table->opts.title.text && table->opts.title.pos == SC_POSITION_TOP) {
            render_title_line(table, inner_w, 1);
        } else if (bs != SC_BORDER_NONE) {
            render_horizontal_border(table, cw, tbc[bs].tl, tbc[bs].tr,
                         tbc[bs].h, oc, tbc[bs].t_top, oc, oc, 0, NULL);
        }
    }

    /* ── header row ── */
    if (table->opts.header.row) {
        ScCell *hcells = malloc(table->column_count * sizeof(ScCell));
        for (size_t c = 0; c < table->column_count; c++)
            hcells[c] = sc_cell(table->columns[c].header ? table->columns[c].header : "");
        render_row(table, hcells, table->opts.header.row_bg, 1, cw, 0, NULL);
        free(hcells);

        if (bs != SC_BORDER_NONE) {
            ScColor hc = (hrsc.index != -2) ? hrsc : ic;
            render_horizontal_border(table, cw, tbc[bs].t_left, tbc[bs].t_right,
                         tbc[bs].h, hc, tbc[bs].cross, hc, oc, 1, NULL);
        }
    }

    /* ── data rows ── */
    size_t max_r = table->row_count;
    if (table->opts.max_rows > 0 && (size_t)table->opts.max_rows < max_r)
        max_r = (size_t)table->opts.max_rows;

    for (size_t r = 0; r < max_r; r++) {
        /* Update RSCtx: initialize new spans, clear non-continuations */
        for (size_t c = 0; c < table->column_count; c++) {
            int rs = table->rows[r].cells[c].rowspan;
            if (rs > 1) {
                int sep_h = (bs != SC_BORDER_NONE && !no_inner_h) ? 1 : 0;
                int sh = 0;
                for (int k = 0; k < rs && r + (size_t)k < table->row_count; k++) {
                    if (k > 0) sh += sep_h;
                    sh += row_h_arr[r + k];
                }
                ScVAlign va = table->columns[c].opts.valign;
                if (table->rows[r].cells[c].valign_set) va = table->rows[r].cells[c].valign;
                rsc[c] = (RSCtx){ &table->rows[r].cells[c], sh, 0, va };
            } else if (rs != -1) {
                rsc[c] = (RSCtx){ NULL, 0, 0, 0 };
            }
        }

        if (r > 0 && bs != SC_BORDER_NONE && !no_inner_h) {
            /* Compute rowspan state: cols with SC_ROW_SKIP get blank separator */
            for (size_t c = 0; c < table->column_count; c++)
                is_rs[c] = (table->rows[r].cells[c].rowspan == -1);
            render_inner_sep(table, cw, ic, is_rs, rsc);
            /* Separator is 1 visual line within already-running spans only */
            for (size_t c = 0; c < table->column_count; c++)
                if (is_rs[c] && rsc[c].cell) rsc[c].vis_offset++;
        }
        ScColor row_bg = table->rows[r].bg;
        if (row_bg.index == -2) {
            row_bg = SC_ANSI_COLOR_NONE;
            if (table->opts.striped && (r % 2 == 1)) row_bg = table->opts.stripe_bg;
        }
        render_row(table, table->rows[r].cells, row_bg, 0, cw, row_h_arr[r], rsc);

        /* Advance vis_offset for all active spans */
        for (size_t c = 0; c < table->column_count; c++)
            if (rsc[c].cell) rsc[c].vis_offset += row_h_arr[r];
    }

    /* ── truncation indicator ── */
    if (table->opts.max_rows > 0 && table->row_count > max_r) {
        char msg[64];
        snprintf(msg, sizeof(msg), "… %zu more rows", table->row_count - max_r);
        ScTextStyle dim = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        ScCell *ind = malloc(table->column_count * sizeof(ScCell));
        ind[0] = sc_cell_cs(msg, (int)table->column_count);
        for (size_t c = 1; c < table->column_count; c++) ind[c] = sc_cell_skip();

        ScTable *mt = (ScTable *)table;
        ScTextStyle saved_hopts = mt->opts.header.opts;
        mt->opts.header.opts = dim;
        render_row(table, ind, SC_ANSI_COLOR_NONE, 1, cw, 0, NULL);
        mt->opts.header.opts = saved_hopts;
        free(ind);
    }

    /* ── footer rows ── */
    if (table->footer_rows_count > 0 && bs != SC_BORDER_NONE) {
        ScColor hc = (hrsc.index != -2) ? hrsc : ic;
        render_horizontal_border(table, cw, tbc[bs].t_left, tbc[bs].t_right,
                     tbc[bs].h, hc, tbc[bs].cross, hc, oc, 1, NULL);
    }
    for (size_t r = 0; r < table->footer_rows_count; r++) {
        if (r > 0 && bs != SC_BORDER_NONE && !no_inner_h) {
            render_horizontal_border(table, cw, tbc[bs].t_left, tbc[bs].t_right,
                         tbc[bs].h, ic, tbc[bs].cross, ic, oc, 1, NULL);
        }
        render_row(table, table->footer_rows[r].cells, table->opts.footer.row_bg, 2, cw, 0, NULL);
    }

    /* ── bottom border ── */
    if (!no_outer) {
        if (table->opts.title.text && table->opts.title.pos == SC_POSITION_BOTTOM) {
            render_title_line(table, inner_w, 0);
        } else if (bs != SC_BORDER_NONE) {
            render_horizontal_border(table, cw, tbc[bs].bl, tbc[bs].br,
                         tbc[bs].h, oc, tbc[bs].t_bot, oc, oc, 0, NULL);
        }
    }

    free(is_rs);
    free(rsc);
    free(row_h_arr);
    free(cw);
    for (int i = 0; i < table->opts.margin.bottom; i++) fputc('\n', stdout);
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

/* ── Column width computation ────────────────────────────────────────────── */

static void compute_col_widths(ScTable *t) {
    for (size_t c = 0; c < t->column_count; c++) {
        const char *hdr = t->columns[c].header;
        size_t max_w = hdr ? sc_utf8_string_length(hdr, strlen(hdr)) : 0;

        /* only consider cells with colspan <= 1 (not spanning cells) */
        for (size_t r = 0; r < t->row_count; r++) {
            if (c >= t->rows[r].ncols) continue;
            ScCell *cell = &t->rows[r].cells[c];
            if (cell->colspan != 0 && cell->colspan != 1) continue; /* skip/span */
            if (cell->rowspan == -1) continue;
            size_t w = cell_vis_width(cell);
            if (w > max_w) max_w = w;
        }
        for (size_t r = 0; r < t->footer_rows_count; r++) {
            if (c >= t->footer_rows[r].ncols) continue;
            ScCell *cell = &t->footer_rows[r].cells[c];
            if (cell->colspan != 0 && cell->colspan != 1) continue;
            if (cell->rowspan == -1) continue;
            size_t w = cell_vis_width(cell);
            if (w > max_w) max_w = w;
        }

        int col_w = (int)max_w + t->opts.cell_pad.left + t->opts.cell_pad.right;
        ScColOpts *co = &t->columns[c].opts;
        if (co->fixed_width > 0)       col_w = co->fixed_width;
        else {
            if (co->min_width > 0 && col_w < co->min_width) col_w = co->min_width;
            if (co->max_width > 0 && col_w > co->max_width) col_w = co->max_width;
        }
        t->columns[c].width = col_w;
    }
}

/* Scale flex columns to reach total_width. */
static void apply_total_width(ScTable *t) {
    if (t->opts.total_width <= 0) return;
    ScBorderType bs = t->opts.borders.style;
    int no_outer = t->opts.borders.no_outer;

    int outer = (bs != SC_BORDER_NONE && !no_outer) ? 2 : 0;
    int seps = 0;
    for (size_t c = 0; c + 1 < t->column_count; c++) {
        int is_hcol = (t->opts.header.col && c == 0);
        if (bs != SC_BORDER_NONE && (!t->opts.borders.no_inner_v || is_hcol))
            seps++;
    }
    int current = outer + seps;
    for (size_t c = 0; c < t->column_count; c++) current += t->columns[c].width;

    int delta = t->opts.total_width - current;
    if (delta == 0) return;

    /* count flex columns */
    int nflex = 0;
    for (size_t c = 0; c < t->column_count; c++)
        if (t->columns[c].opts.fixed_width == 0) nflex++;
    if (nflex == 0) return;

    int per = delta / nflex;
    int rem = delta - per * nflex;
    int sign = (rem >= 0) ? 1 : -1;
    rem = (rem < 0) ? -rem : rem;

    for (size_t c = 0; c < t->column_count; c++) {
        if (t->columns[c].opts.fixed_width > 0) continue;
        int adj = per + (rem > 0 ? sign : 0);
        if (rem > 0) rem--;
        int new_w = t->columns[c].width + adj;
        if (new_w < 2) new_w = 2;
        t->columns[c].width = new_w;
    }
}

static int table_inner_w(const ScTable *t) {
    int w = 0;
    for (size_t c = 0; c < t->column_count; c++) {
        w += t->columns[c].width;
        if (c < t->column_count - 1 && t->opts.borders.style != SC_BORDER_NONE) {
            int is_hcol = (t->opts.header.col && c == 0);
            if (!t->opts.borders.no_inner_v || is_hcol) w++;
        }
    }
    return w;
}

/* Pre-compute the visual height of each data row (spanning cells excluded). */
static int *compute_row_heights(const ScTable *t, const int *cw) {
    int cpy   = t->opts.cell_pad.top + t->opts.cell_pad.bottom;
    int cpx_v = t->opts.cell_pad.left + t->opts.cell_pad.right;
    int *rh   = malloc(t->row_count * sizeof(int));
    for (size_t r = 0; r < t->row_count; r++) {
        size_t max_c = 0;
        for (size_t c = 0; c < t->column_count; c++) {
            const ScCell *cell = &t->rows[r].cells[c];
            if (cell->colspan < 0) continue;
            if (cell->rowspan < 0 || cell->rowspan > 1) continue;
            int cw_c = cw[c] - cpx_v;
            if (cw_c < 0) cw_c = 0;
            size_t nl;
            TLine *lines = (t->columns[c].opts.word_wrap && cw_c > 0)
                ? wrap_cell_lines(cell, cw_c, &nl)
                : make_cell_lines(cell, &nl);
            free_tlines(lines, nl);
            if (nl > max_c) max_c = nl;
        }
        rh[r] = (int)max_c + cpy;
        if (rh[r] < 1) rh[r] = 1;
    }
    return rh;
}

static void tpre(const ScTable *t) {
    for (int i = 0; i < t->opts.margin.left; i++) fputc(' ', stdout);
}

/* ── Horizontal rule rendering ───────────────────────────────────────────── */

static void render_horizontal_border(const ScTable *t, int *col_widths,
                          const char *lc, const char *rc,
                          const char *fill, ScColor fill_color,
                          const char *mid,  ScColor mid_color,
                          ScColor edge_color, int use_hcol,
                          const int *rs) {  /* rs[c]=1 → col c has active rowspan */
    ScBorderType bs = t->opts.borders.style;
    int rtl = t->opts.rtl;
    int no_outer = t->opts.borders.no_outer;

    /* Adjust outer corners when first/last rendered col has rowspan */
    if (rs && !no_outer) {
        size_t fc = rtl ? (t->column_count - 1) : 0;
        size_t lcc = rtl ? 0 : (t->column_count - 1);
        if (rs[fc])  lc = tbc[bs].v;
        if (rs[lcc]) rc = tbc[bs].v;
    }

    tpre(t);
    if (!no_outer) print_ch(lc, edge_color);

    for (size_t ci = 0; ci < t->column_count; ci++) {
        size_t c = rtl ? (t->column_count - 1 - ci) : ci;

        if (rs && rs[c]) {
            for (int i = 0; i < col_widths[c]; i++) fputc(' ', stdout);
        } else {
            sc_apply_colors(fill_color, SC_ANSI_COLOR_NONE);
            for (int i = 0; i < col_widths[c]; i++) fputs(fill, stdout);
            fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
        }

        if (ci < t->column_count - 1 && mid) {
            size_t hcol_phys = rtl ? (t->column_count - 1) : 0;
            int is_hcol = (t->opts.header.col && c == hcol_phys);
            int has_vsep = !t->opts.borders.no_inner_v || is_hcol;
            if (has_vsep) {
                /* Determine junction char considering rowspan on either side */
                const char *jchar = mid;
                if (rs) {
                    size_t nc = rtl ? (c - 1) : (c + 1);
                    int cur_rs  = rs[c];
                    int next_rs = rs[nc];
                    if      (cur_rs && next_rs) jchar = tbc[bs].v;
                    else if (cur_rs)            jchar = tbc[bs].t_left;
                    else if (next_rs)           jchar = tbc[bs].t_right;
                }
                ScColor jc = mid_color;
                if (use_hcol && is_hcol
                        && t->opts.borders.header_col_sep_color.index != -2)
                    jc = t->opts.borders.header_col_sep_color;
                print_ch(jchar, jc);
            }
        }
    }

    if (!no_outer) print_ch(rc, edge_color);
    fputc('\n', stdout);
}

/* Inner row separator that can show spanning-cell content in spanned columns. */
static void render_inner_sep(const ScTable *t, int *cw,
                              ScColor ic, const int *is_rs, const RSCtx *rsc) {
    ScBorderType bs    = t->opts.borders.style;
    ScColor oc          = t->opts.borders.outer_color;
    int no_outer        = t->opts.borders.no_outer;
    int no_inner_v      = t->opts.borders.no_inner_v;
    int rtl             = t->opts.rtl;
    int cpxl            = t->opts.cell_pad.left;
    int cpxr            = t->opts.cell_pad.right;
    int cpx             = cpxl + cpxr;
    size_t hcol_phys    = rtl ? (t->column_count - 1) : 0;

    const char *lc = tbc[bs].t_left;
    const char *rc = tbc[bs].t_right;
    if (is_rs && !no_outer) {
        size_t fc  = rtl ? (t->column_count - 1) : 0;
        size_t lcc = rtl ? 0 : (t->column_count - 1);
        if (is_rs[fc])  lc = tbc[bs].v;
        if (is_rs[lcc]) rc = tbc[bs].v;
    }
    tpre(t);
    if (!no_outer) print_ch(lc, oc);

    for (size_t ci = 0; ci < t->column_count; ci++) {
        size_t c = rtl ? (t->column_count - 1 - ci) : ci;

        if (is_rs && is_rs[c]) {
            /* Spanning column: try to render cell content at this line */
            ScColor col_bg = t->columns[c].opts.bg;
            if (rsc && rsc[c].cell) {
                int content_w = cw[c] - cpx;
                if (content_w < 0) content_w = 0;
                size_t ncl;
                TLine *cl = (t->columns[c].opts.word_wrap && content_w > 0)
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
                    ScHAlign ha = t->columns[c].opts.halign;
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
                print_spaces_bg(cw[c], col_bg);
            }
        } else {
            sc_apply_colors(ic, SC_ANSI_COLOR_NONE);
            for (int i = 0; i < cw[c]; i++) fputs(tbc[bs].h, stdout);
            fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
        }

        /* Junction between columns */
        if (ci < t->column_count - 1) {
            int is_hcol = (t->opts.header.col && c == hcol_phys);
            int has_vsep = !no_inner_v || is_hcol;
            if (has_vsep) {
                size_t nc = rtl ? (c - 1) : (c + 1);
                int cur_rs  = is_rs ? is_rs[c]  : 0;
                int next_rs = is_rs ? (int)is_rs[nc] : 0;
                const char *jchar = tbc[bs].cross;
                if      (cur_rs && next_rs) jchar = tbc[bs].v;
                else if (cur_rs)            jchar = tbc[bs].t_left;
                else if (next_rs)           jchar = tbc[bs].t_right;
                ScColor jc = ic;
                if (is_hcol && t->opts.borders.header_col_sep_color.index != -2)
                    jc = t->opts.borders.header_col_sep_color;
                print_ch(jchar, jc);
            }
        }
    }

    if (!no_outer) print_ch(rc, oc);
    fputc('\n', stdout);
}

/* Title line (replaces top/bottom outer line when title is set) */
static void render_title_line(const ScTable *t, int inner_w, int is_top) {
    ScBorderType bs = t->opts.borders.style;
    ScColor oc = t->opts.borders.outer_color;
    const char *lc = is_top ? tbc[bs].tl : tbc[bs].bl;
    const char *rc = is_top ? tbc[bs].tr : tbc[bs].br;
    const char *h  = tbc[bs].h;
    int tpad = t->opts.title.pad > 0 ? t->opts.title.pad : 1;

    tpre(t);
    print_ch(lc, oc);
    if (t->opts.title.text && *t->opts.title.text) {
        int tlen   = (int)strlen(t->opts.title.text);
        int dashes = inner_w - tlen - 2 * tpad;
        if (dashes < 0) dashes = 0;
        int ld = 1, rd = dashes - 1;
        if (t->opts.title.align == SC_ALIGN_CENTER) { ld = dashes/2; rd = dashes - ld; }
        else if (t->opts.title.align == SC_ALIGN_RIGHT) { ld = dashes - 1; rd = 1; }
        if (ld < 0) ld = 0; if (rd < 0) rd = 0;
        sc_apply_colors(oc, SC_ANSI_COLOR_NONE);
        for (int i = 0; i < ld; i++) fputs(h, stdout);
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
        for (int i = 0; i < tpad; i++) print_ch(" ", oc);
        sc_print(t->opts.title.text, t->opts.title.style);
        for (int i = 0; i < tpad; i++) print_ch(" ", oc);
        sc_apply_colors(oc, SC_ANSI_COLOR_NONE);
        for (int i = 0; i < rd; i++) fputs(h, stdout);
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    } else {
        sc_apply_colors(oc, SC_ANSI_COLOR_NONE);
        for (int i = 0; i < inner_w; i++) fputs(h, stdout);
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    }
    print_ch(rc, oc);
    fputc('\n', stdout);
}

/* ── Row rendering ───────────────────────────────────────────────────────── */

/* row_kind: 0=data, 1=header, 2=footer
   row_h_in: pre-computed height (>0 for data rows); 0 = compute from cells
   rsc:      rowspan context array (NULL for header/footer rows) */
static void render_row(const ScTable *t, const ScCell *cells,
                        ScColor row_bg, int row_kind, int *col_widths,
                        int row_h_in, const RSCtx *rsc) {
    ScBorderType bs  = t->opts.borders.style;
    ScColor oc  = t->opts.borders.outer_color;
    ScColor ic  = t->opts.borders.inner_color;
    int no_outer   = t->opts.borders.no_outer;
    int no_inner_v = t->opts.borders.no_inner_v;
    int rtl        = t->opts.rtl;
    int cpx        = t->opts.cell_pad.left + t->opts.cell_pad.right;
    int cpxl       = t->opts.cell_pad.left;
    int cpxr       = t->opts.cell_pad.right;
    int cpy        = t->opts.cell_pad.top + t->opts.cell_pad.bottom;
    int cpt        = t->opts.cell_pad.top;
    int is_hdr     = (row_kind == 1);
    int is_ftr     = (row_kind == 2);

    /* physical header col index */
    size_t hcol_phys = rtl ? (t->column_count - 1) : 0;

    /* build cell lines (with wrap if enabled) */
    TLine **cl  = malloc(t->column_count * sizeof(TLine *));
    size_t *cnl = malloc(t->column_count * sizeof(size_t));
    size_t max_content = 0;

    /* Map logical render order → physical column index */
    for (size_t ci = 0; ci < t->column_count; ci++) {
        size_t c = rtl ? (t->column_count - 1 - ci) : ci;

        /* skip colspan-covered cells */
        int cs = cells[c].colspan;
        if (cs < 0) { cl[c] = NULL; cnl[c] = 0; continue; }

        /* rowspan continuation: build lines from spanning cell via RSCtx */
        if (cells[c].rowspan == -1) {
            if (rsc && rsc[c].cell) {
                int span_cw = col_widths[c] - cpx;
                if (span_cw < 0) span_cw = 0;
                if (t->columns[c].opts.word_wrap && span_cw > 0)
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
        if ((int)c + ecs > (int)t->column_count) ecs = (int)t->column_count - (int)c;

        /* compute effective content width for this cell/span */
        int span_total = 0;
        for (int k = 0; k < ecs; k++) {
            size_t cc = c + (size_t)k;
            span_total += col_widths[cc];
            if (k < ecs - 1) {
                int is_hcol_k = (t->opts.header.col && cc == hcol_phys);
                if (!no_inner_v || is_hcol_k) span_total++; /* inner sep */
            }
        }
        int cw = span_total - cpx;
        if (cw < 0) cw = 0;

        if (t->columns[c].opts.word_wrap && cw > 0)
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
        tpre(t);
        if (!no_outer) print_ch(tbc[bs].v, oc);

        size_t ci = 0;
        while (ci < t->column_count) {
            size_t c = rtl ? (t->column_count - 1 - ci) : ci;

            int cs_raw = cells[c].colspan;
            if (cs_raw < 0) { ci++; continue; } /* SKIP */

            int ecs = (cs_raw > 1) ? cs_raw : 1;
            if ((int)c + ecs > (int)t->column_count) ecs = (int)t->column_count - (int)c;

            /* per-cell background */
            ScColor col_bg  = t->columns[c].opts.bg;
            ScColor cell_bg = (row_bg.index != -2) ? row_bg : col_bg;
            int is_hcol_c = (t->opts.header.col && c == hcol_phys);
            if (is_hcol_c && !is_hdr) {
                ScColor hcol_bg = is_ftr ? t->opts.footer.col_bg : t->opts.header.col_bg;
                cell_bg = hcol_bg;
            }
            if (is_hdr)  cell_bg = t->opts.header.row_bg;
            if (is_ftr && !is_hcol_c) cell_bg = t->opts.footer.row_bg;
            if (is_ftr &&  is_hcol_c) cell_bg = t->opts.footer.col_bg;

            /* alignment */
            ScHAlign  ha = t->columns[c].opts.halign;
            ScVAlign va = t->columns[c].opts.valign;
            if (cells[c].align_set)  ha = cells[c].align;
            if (cells[c].valign_set) va = cells[c].valign;

            /* effective content width for this span */
            int span_total = 0;
            for (int k = 0; k < ecs; k++) {
                size_t cc = c + (size_t)k;
                span_total += col_widths[cc];
                if (k < ecs - 1) {
                    int is_hcol_k = (t->opts.header.col && cc == hcol_phys);
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
                            if (so.attr == 0) so.attr = t->opts.header.opts.attr;
                            if (so.fg.index == -2) so.fg = t->opts.header.opts.fg;
                        } else if (is_ftr) {
                            if (so.attr == 0) so.attr = t->opts.footer.opts.attr;
                            if (so.fg.index == -2) so.fg = t->opts.footer.opts.fg;
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
                            if (so.attr == 0) so.attr = t->opts.header.opts.attr;
                            if (so.fg.index == -2) so.fg = t->opts.header.opts.fg;
                        } else if (is_ftr) {
                            if (so.attr == 0) so.attr = t->opts.footer.opts.attr;
                            if (so.fg.index == -2) so.fg = t->opts.footer.opts.fg;
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
            if (ci + (size_t)ecs < t->column_count && bs != SC_BORDER_NONE) {
                int is_hcol_end = (t->opts.header.col && end_col == hcol_phys);
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

    for (size_t c = 0; c < t->column_count; c++)
        if (cl[c]) free_tlines(cl[c], cnl[c]);
    free(cl);
    free(cnl);
}







void sc_table_free(ScTable *table) {
    for (size_t i = 0; i < table->column_count; i++) {
        free(table->columns[i].header);
    }
    free(table->columns);
    free_row_array(table->rows, table->row_count);
    free_row_array(table->footer_rows, table->footer_rows_count);
    free(table);
}

/**
 * Frees all resources owned by a single row.
 *
 * Releases the `ScText` of every `SC_CELL_MARKUP` cell, then frees the
 * cells array. Does not free the `TRow` struct itself.
 *
 * @param row Pointer to the row to free.
 */
static void free_row(TRow *row) {
    for (size_t j = 0; j < row->ncols; j++) {
        if (row->cells[j].kind == SC_CELL_MARKUP && row->cells[j].text) {
            sc_text_free(row->cells[j].text);
        }
    }
    free(row->cells);
}

/**
 * Frees an array of rows and the array itself.
 *
 * Calls `free_row` on each element, then frees the array pointer.
 *
 * @param rows  Pointer to the row array; may be `NULL`.
 * @param count Number of rows in the array.
 */
static void free_row_array(TRow *rows, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free_row(&rows[i]);
    }
    free(rows);
}


