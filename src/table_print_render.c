#pragma once

#include "sparcli.h"         // IWYU pragma: export
#include "internal.h"        // IWYU pragma: export
#include "table_internal.h"  // IWYU pragma: export
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Token type for the word-wrap subsystem: one contiguous run of spaces or
   non-spaces extracted from a TLine span. */
typedef struct { char *s; size_t vis_w; ScTextStyle opts; int is_space; } WwTok;

/* Mutable accumulator for the word-wrap line-building loop. */
typedef struct {
    TLine  *res;   /* completed output lines */
    size_t  nres;
    size_t  rcap;
    TSpan  *sbuf;  /* spans accumulating for the current output line */
    size_t  sn;    /* span count */
    size_t  sc2;   /* span buffer capacity */
    size_t  sw;    /* visible width of the current output line */
} WwAccum;

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
    static void render_title_line       (const Table *table, int is_top);
    static void render_title_inner      (const Table *table, const char *h, ScColor oc, int tpad);
    static void render_title_with_fill  (const Table *table, const char *h, ScColor oc, int tpad);
    static void compute_title_fill_split(int inner_w, int tlen, int tpad, ScHAlign align, int *ld, int *rd);
    static void render_title_full_fill  (int inner_w, const char *h, ScColor oc);
    static void render_row(
        const Table *table, const ScCell *cells,
        ScColor row_bg, int row_kind, int row_h_in
    );
        static void    free_tlines    (TLine *lines, size_t n);
        static void    flush_tline    (TLine **lines, size_t *cap, size_t *n,
                                       TSpan *buf, size_t buf_n, size_t vis_w);
        static TLine  *make_cell_lines      (const ScCell *cell, size_t *out_n);
        static void    resolve_cell_span    (const ScCell *cell, size_t si, const char **out_s, ScTextStyle *out_opts);
        static void    scan_text_for_newlines(const char *s, ScTextStyle opts, TLine **lines, size_t *lines_cap, size_t *nlines, TSpan **buf, size_t *buf_cap, size_t *buf_n, size_t *buf_w);
        static void    buf_push_segment     (TSpan **buf, size_t *buf_cap, size_t *buf_n, size_t *buf_w, const char *start, size_t len, ScTextStyle opts);
        static size_t  cell_vis_width       (const ScCell *cell);
        static TLine  *wrap_one_tline      (const TLine *src, int wrap_w, size_t *out_n);
        static void    tokenize_tline_spans(const TLine *src, WwTok **out_toks, size_t *out_n);
        static void    emit_space_tok      (WwAccum *a, WwTok *tok, const WwTok *next, int wrap_w);
        static void    emit_word_tok       (WwAccum *a, WwTok *tok, int wrap_w);
        static void    hard_break_word     (WwAccum *a, WwTok *tok, int wrap_w);
        static void    ww_flush_sbuf       (WwAccum *a);
        static void    ww_push_span        (WwAccum *a, char *s, size_t vis_w, ScTextStyle opts);
        static TLine  *wrap_cell_lines     (const ScCell *cell, int wrap_w, size_t *out_n);
        static void    append_tline_copy   (TLine **res, size_t *nres, size_t *rcap, const TLine *src);
        static void    append_wrapped_lines    (TLine **res, size_t *nres, size_t *rcap, const TLine *src, int wrap_w);
        static size_t  build_row_cell_lines         (const Table *table, const ScCell *cells, const RowRCtx *rctx, TLine **cl, size_t *cnl);
        static void    build_rowspan_cont_col_lines (const Table *table, size_t c, int cpx, const RowRCtx *rctx, TLine **cl, size_t *cnl);
        static size_t  build_span_col_lines         (const Table *table, const ScCell *cell, size_t c, int cpx, TLine **cl, size_t *cnl);
        static int     compute_span_w               (const Table *table, size_t c, int ecs);
        static ScColor resolve_cell_bg         (const Table *table, size_t c, ScColor row_bg, int row_kind);
        static void    resolve_cell_align      (const Table *table, const ScCell *cells, size_t c, ScHAlign *ha, ScVAlign *va);
        static int     compute_rowspan_cell_cli(const RSCtx *rsc_c, int li, int cn);
        static int     compute_normal_cell_cli (int li, int cn, ScVAlign va, const RowRCtx *rctx);
        static void    render_row_visual_line  (const Table *table, const ScCell *cells, TLine **cl, size_t *cnl, int li, const RowRCtx *rctx);
        static void    render_row_cell         (const Table *table, const ScCell *cells, TLine **cl, size_t *cnl, size_t c, size_t ci, int ecs, int li, const RowRCtx *rctx);
        static ScTextStyle apply_hdrftr_style  (ScTextStyle so, const ScTableOpts *opts, const RowRCtx *rctx);
        static void    render_cell_line         (const TLine *line, int cw, ScHAlign ha, ScColor cell_bg, const ScTableOpts *opts, const RowRCtx *rctx);
        static void    print_cell_line_aligned  (const TLine *line, int rw, ScHAlign ha, ScColor cell_bg, const ScTableOpts *opts, const RowRCtx *rctx);
        static void    print_cell_line_truncated(const TLine *line, int cw, ScColor cell_bg, const ScTableOpts *opts, const RowRCtx *rctx);
        static void    render_row_vsep          (const Table *table, size_t c, size_t ci, int ecs);


/* Top-level render sequence: margins → border → header → data → footer → margins. */
static void table_render(Table *table) {
    print_empty_lines(table->opts.margin.top);
    render_top_border(table);
    render_header_row(table);
    render_data_rows(table);
    render_footer_rows(table);
    render_bottom_border(table);
    print_empty_lines(table->opts.margin.bottom);
}



/* ── TLine memory helpers ────────────────────────────────────────────────── */

/* Frees all span strings inside each TLine, then the spans array and the
   lines array itself. */
static void free_tlines(TLine *lines, size_t n) {
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < lines[i].count; j++)
            free((char *)lines[i].spans[j].text);
        free(lines[i].spans);
    }
    free(lines);
}

/* Copies the span buffer into a new TLine entry and appends it to the lines
   array, growing the array if needed. */
static void flush_tline(TLine **lines, size_t *cap, size_t *n,
                         TSpan *buf, size_t buf_n, size_t vis_w) {
    /* --- grow the lines array if full --- */
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 4;
        *lines = realloc(*lines, *cap * sizeof(TLine));
    }
    /* --- copy the span buffer into a new owned TLine --- */
    TSpan *ls = malloc((buf_n + 1) * sizeof(TSpan));
    if (buf_n) memcpy(ls, buf, buf_n * sizeof(TSpan));
    (*lines)[(*n)++] = (TLine){ ls, buf_n, vis_w };
}

/* ── Cell line building ───────────────────────────────────────────────────── */

/** Splits the content of @p cell on '\\n' into an array of TLines. Each TLine
 *  owns heap copies of its span strings. Delegates span resolution and newline
 *  scanning to resolve_cell_span() and scan_text_for_newlines(). */
static TLine *make_cell_lines(const ScCell *cell, size_t *out_n) {
    size_t lines_cap = 4, nlines = 0;
    TLine *lines = malloc(lines_cap * sizeof(TLine));
    size_t buf_cap = 4, buf_n = 0, buf_w = 0;
    TSpan *buf = malloc(buf_cap * sizeof(TSpan));

    size_t nspans = ((cell->kind == SC_CELL_TEXT || cell->kind == SC_CELL_MARKUP) && cell->text)
        ? cell->text->count : 1;

    for (size_t si = 0; si < nspans; si++) {
        const char *s;
        ScTextStyle opts;
        resolve_cell_span(cell, si, &s, &opts);
        scan_text_for_newlines(s, opts, &lines, &lines_cap, &nlines, &buf, &buf_cap, &buf_n, &buf_w);
    }

    flush_tline(&lines, &lines_cap, &nlines, buf, buf_n, buf_w);
    free(buf);
    *out_n = nlines;
    return lines;
}

/** Resolves the raw string and text style for span index @p si of @p cell.
 *  SC_CELL_STR cells have exactly one span and use a plain (unstyled) style. */
static void resolve_cell_span(const ScCell *cell, size_t si,
                               const char **out_s, ScTextStyle *out_opts) {
    static const ScTextStyle PLAIN = { 0, { -2,0,0,0 }, { -2,0,0,0 } };
    if (cell->kind == SC_CELL_STR) {
        *out_s    = cell->str ? cell->str : "";
        *out_opts = PLAIN;
    } else {
        *out_s    = cell->text->spans[si].raw_str;
        *out_opts = cell->text->spans[si].style;
    }
}

/** Scans @p s for '\\n', calling buf_push_segment() for each text segment and
 *  flush_tline() on each newline. Remaining text after the last newline is
 *  left in the buffer for the caller to flush. */
static void scan_text_for_newlines(const char *s, ScTextStyle opts,
                                    TLine **lines, size_t *lines_cap, size_t *nlines,
                                    TSpan **buf, size_t *buf_cap, size_t *buf_n, size_t *buf_w) {
    const char *start = s;
    while (*s) {
        if (*s == '\n') {
            size_t len = (size_t)(s - start);
            if (len > 0)
                buf_push_segment(buf, buf_cap, buf_n, buf_w, start, len, opts);
            flush_tline(lines, lines_cap, nlines, *buf, *buf_n, *buf_w);
            *buf_n = 0; *buf_w = 0;
            start = s + 1;
        }
        s++;
    }
    size_t len = (size_t)(s - start);
    if (len > 0)
        buf_push_segment(buf, buf_cap, buf_n, buf_w, start, len, opts);
}

/** Appends the text segment [@p start, @p start+@p len) with style @p opts to
 *  the span buffer, doubling its capacity if it is full. */
static void buf_push_segment(TSpan **buf, size_t *buf_cap, size_t *buf_n, size_t *buf_w,
                              const char *start, size_t len, ScTextStyle opts) {
    if (*buf_n == *buf_cap) {
        *buf_cap *= 2;
        *buf = realloc(*buf, *buf_cap * sizeof(TSpan));
    }
    (*buf)[(*buf_n)++] = (TSpan){ strndup(start, len), opts };
    *buf_w += sc_utf8_string_length(start, len);
}

/* Returns the visible column width of the widest line in a cell. */
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

/** Wraps a single TLine into multiple TLines each fitting within @p wrap_w cols.
 *  Tokenizes the line, dispatches each token to emit_space_tok() or
 *  emit_word_tok(), then flushes the final accumulated line. */
static TLine *wrap_one_tline(const TLine *src, int wrap_w, size_t *out_n) {
    WwTok *toks; size_t ntok;
    tokenize_tline_spans(src, &toks, &ntok);

    WwAccum acc = {0};
    for (size_t ti = 0; ti < ntok; ti++) {
        const WwTok *next = (ti + 1 < ntok) ? &toks[ti + 1] : NULL;
        if (toks[ti].is_space)
            emit_space_tok(&acc, &toks[ti], next, wrap_w);
        else
            emit_word_tok(&acc, &toks[ti], wrap_w);
    }
    ww_flush_sbuf(&acc);

    TLine *res = acc.res;
    free(acc.sbuf);
    for (size_t ti = 0; ti < ntok; ti++) if (toks[ti].s) free(toks[ti].s);
    free(toks);
    *out_n = acc.nres;
    return res;
}

/** Splits each span of @p src into alternating non-space/space-run WwTok tokens.
 *  The caller owns the returned array and must free each token's @c s field. */
static void tokenize_tline_spans(const TLine *src, WwTok **out_toks, size_t *out_n) {
    WwTok *toks = NULL; size_t ntok = 0, tcap = 0;
    for (size_t si = 0; si < src->count; si++) {
        const char *p = src->spans[si].text;
        ScTextStyle opts = src->spans[si].opts;
        while (*p) {
            const char *seg = p;
            int is_sp = (*p == ' ');
            while (*p && ((*p == ' ') == is_sp)) p++;
            size_t len = (size_t)(p - seg);
            size_t vw  = is_sp ? len : sc_utf8_string_length(seg, len);
            if (ntok == tcap) { tcap = tcap ? tcap*2 : 16; toks = realloc(toks, tcap*sizeof(WwTok)); }
            toks[ntok++] = (WwTok){ strndup(seg, len), vw, opts, is_sp };
        }
    }
    *out_toks = toks;
    *out_n    = ntok;
}

/** Emits a space token: drops it when at line start, flushes and wraps when
 *  the following word won't fit after it, otherwise appends it to the line. */
static void emit_space_tok(WwAccum *a, WwTok *tok, const WwTok *next, int wrap_w) {
    if (a->sn == 0) { free(tok->s); tok->s = NULL; return; }
    if (next && !next->is_space
            && (int)(a->sw + tok->vis_w + next->vis_w) > wrap_w) {
        free(tok->s); tok->s = NULL;
        ww_flush_sbuf(a);
        return;
    }
    ww_push_span(a, tok->s, tok->vis_w, tok->opts);
    tok->s = NULL;
}

/** Emits a word token: flushes the current line first if the word doesn't fit,
 *  then hard-breaks it when it exceeds @p wrap_w, otherwise appends it. */
static void emit_word_tok(WwAccum *a, WwTok *tok, int wrap_w) {
    if (a->sn > 0 && (int)(a->sw + tok->vis_w) > wrap_w)
        ww_flush_sbuf(a);
    if ((int)tok->vis_w > wrap_w)
        hard_break_word(a, tok, wrap_w);
    else {
        ww_push_span(a, tok->s, tok->vis_w, tok->opts);
        tok->s = NULL;
    }
}

/** Splits a word token wider than @p wrap_w into fit-sized chunks, flushing
 *  a new output line after each full chunk. Frees @p tok->s when done. */
static void hard_break_word(WwAccum *a, WwTok *tok, int wrap_w) {
    const char *p = tok->s;
    size_t rem = strlen(tok->s);
    while (rem > 0) {
        int avail = wrap_w - (int)a->sw;
        if (avail <= 0) { ww_flush_sbuf(a); avail = wrap_w; }
        size_t cb = sc_utf8_trim_to_cols(p, avail);
        if (cb == 0) break;
        size_t cw2 = sc_utf8_string_length(p, cb);
        ww_push_span(a, strndup(p, cb), cw2, tok->opts);
        p += cb; rem -= cb;
        if (rem > 0) ww_flush_sbuf(a);
    }
    free(tok->s); tok->s = NULL;
}

/** Trims trailing space spans from the current line, flushes it as a new TLine
 *  into @p a->res via flush_tline(), then resets the span count and width. */
static void ww_flush_sbuf(WwAccum *a) {
    while (a->sn > 0 && a->sbuf[a->sn - 1].text[0] == ' ') {
        a->sw -= strlen(a->sbuf[a->sn - 1].text);
        free((char *)a->sbuf[--a->sn].text);
    }
    flush_tline(&a->res, &a->rcap, &a->nres, a->sbuf, a->sn, a->sw);
    a->sn = 0; a->sw = 0;
}

/** Appends span @p s (taking ownership) to the current-line accumulator,
 *  growing the span buffer if needed, and adds @p vis_w to the line width. */
static void ww_push_span(WwAccum *a, char *s, size_t vis_w, ScTextStyle opts) {
    if (a->sn == a->sc2) { a->sc2 = a->sc2 ? a->sc2*2 : 8; a->sbuf = realloc(a->sbuf, a->sc2*sizeof(TSpan)); }
    a->sbuf[a->sn++] = (TSpan){ s, opts };
    a->sw += vis_w;
}

/** Wraps all lines of @p cell that exceed @p wrap_w columns, copying lines
 *  that fit via append_tline_copy() and wrapping wider ones via
 *  append_wrapped_lines(). */
static TLine *wrap_cell_lines(const ScCell *cell, int wrap_w, size_t *out_n) {
    size_t nraw;
    TLine *raw = make_cell_lines(cell, &nraw);
    if (wrap_w <= 0) { *out_n = nraw; return raw; }

    TLine *res = NULL; size_t nres = 0, rcap = 0;
    for (size_t li = 0; li < nraw; li++) {
        if ((int)raw[li].vis_w <= wrap_w)
            append_tline_copy(&res, &nres, &rcap, &raw[li]);
        else
            append_wrapped_lines(&res, &nres, &rcap, &raw[li], wrap_w);
    }
    free_tlines(raw, nraw);
    *out_n = nres;
    return res;
}

/** Deep-copies @p src's spans into a new TLine and appends it to @p res,
 *  growing the array if needed. */
static void append_tline_copy(TLine **res, size_t *nres, size_t *rcap, const TLine *src) {
    if (*nres == *rcap) { *rcap = *rcap ? *rcap*2 : 4; *res = realloc(*res, *rcap*sizeof(TLine)); }
    TSpan *ls = malloc((src->count + 1) * sizeof(TSpan));
    for (size_t j = 0; j < src->count; j++)
        ls[j] = (TSpan){ strdup(src->spans[j].text), src->spans[j].opts };
    (*res)[(*nres)++] = (TLine){ ls, src->count, src->vis_w };
}

/** Wraps @p src via wrap_one_tline() and bulk-appends all resulting TLines into
 *  @p res. The temporary wrap buffer is freed shallowly — span ownership
 *  transfers to @p res. */
static void append_wrapped_lines(TLine **res, size_t *nres, size_t *rcap,
                                  const TLine *src, int wrap_w) {
    size_t nw;
    TLine *wrapped = wrap_one_tline(src, wrap_w, &nw);
    if (*nres + nw > *rcap) {
        while (*rcap < *nres + nw) *rcap = *rcap ? *rcap*2 : 4;
        *res = realloc(*res, *rcap * sizeof(TLine));
    }
    memcpy(*res + *nres, wrapped, nw * sizeof(TLine));
    free(wrapped);
    *nres += nw;
}

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
    if (rctx.row_h < 1) rctx.row_h = 1;

    for (int li = 0; li < rctx.row_h; li++)
        render_row_visual_line(table, cells, cl, cnl, li, &rctx);

    for (size_t c = 0; c < table_data->column_count; c++)
        if (cl[c]) free_tlines(cl[c], cnl[c]);
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
        if ((rs_v == 0 || rs_v == 1) && n > max_content)
            max_content = n;
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
        if (span_cw < 0) span_cw = 0;
        if (table_data->columns[c].opts.word_wrap && span_cw > 0)
            cl[c] = wrap_cell_lines(rctx->rsc[c].cell, span_cw, &cnl[c]);
        else
            cl[c] = make_cell_lines(rctx->rsc[c].cell, &cnl[c]);
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
    if ((int)c + ecs > (int)table_data->column_count)
        ecs = (int)table_data->column_count - (int)c;

    int cw = compute_span_w(table, c, ecs) - cpx;
    if (cw < 0) cw = 0;

    if (table_data->columns[c].opts.word_wrap && cw > 0)
        cl[c] = wrap_cell_lines(cell, cw, &cnl[c]);
    else
        cl[c] = make_cell_lines(cell, &cnl[c]);

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
    if (!no_outer) print_ch(tbc[bs].v, oc);

    size_t ci = 0;
    while (ci < table_data->column_count) {
        size_t c = rtl ? (table_data->column_count - 1 - ci) : ci;
        int cs_raw = cells[c].colspan;
        if (cs_raw < 0) { ci++; continue; }

        int ecs = (cs_raw > 1) ? cs_raw : 1;
        if ((int)c + ecs > (int)table_data->column_count)
            ecs = (int)table_data->column_count - (int)c;

        render_row_cell(table, cells, cl, cnl, c, ci, ecs, li, rctx);
        ci += (size_t)ecs;
    }

    if (!no_outer) print_ch(tbc[bs].v, oc);
    fputc('\n', stdout);
}

/** Renders one cell (padding + content + separator) at visual line @p li.
 *  Resolves background, alignment, and content line index, then delegates. */
static void render_row_cell(const Table *table, const ScCell *cells,
                             TLine **cl, size_t *cnl,
                             size_t c, size_t ci, int ecs, int li,
                             const RowRCtx *rctx) {
    int cw = compute_span_w(table, c, ecs) - (rctx->cpxl + rctx->cpxr);
    if (cw < 0) cw = 0;

    ScColor  cell_bg = resolve_cell_bg(table, c, rctx->row_bg, rctx->row_kind);
    ScHAlign ha; ScVAlign va;
    resolve_cell_align(table, cells, c, &ha, &va);

    int cn        = cl[c] ? (int)cnl[c] : 0;
    int rs_render = cells[c].rowspan;
    int cli;
    if ((rs_render > 1 || rs_render == -1) && rctx->rsc && rctx->rsc[c].cell)
        cli = compute_rowspan_cell_cli(&rctx->rsc[c], li, cn);
    else
        cli = compute_normal_cell_cli(li, cn, va, rctx);

    print_spaces_bg(rctx->cpxl, cell_bg);
    if (cli >= 0 && cli < cn && cl[c])
        render_cell_line(&cl[c][cli], cw, ha, cell_bg, &table->opts, rctx);
    else
        print_spaces_bg(cw, cell_bg);
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
            if (!no_inner_v || is_hcol_k) span_total++;
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

    if (is_hcol_c && !is_hdr)
        cell_bg = is_ftr ? table->opts.footer.col_bg : table->opts.header.col_bg;
    if (is_hdr)               cell_bg = table->opts.header.row_bg;
    if (is_ftr && !is_hcol_c) cell_bg = table->opts.footer.row_bg;
    if (is_ftr &&  is_hcol_c) cell_bg = table->opts.footer.col_bg;

    return cell_bg;
}

/** Resolves the horizontal and vertical alignment for column @p c,
 *  applying per-cell overrides on top of the column defaults. */
static void resolve_cell_align(const Table *table, const ScCell *cells, size_t c,
                                ScHAlign *ha, ScVAlign *va) {
    const ScTableData *table_data = table->table_data;
    *ha = table_data->columns[c].opts.halign;
    *va = table_data->columns[c].opts.valign;
    if (cells[c].align_set)  *ha = cells[c].align;
    if (cells[c].valign_set) *va = cells[c].valign;
}

/** Computes the content line index for a rowspan cell at visual line @p li,
 *  distributing lines across the full span height with vertical alignment. */
static int compute_rowspan_cell_cli(const RSCtx *rsc_c, int li, int cn) {
    int extra = rsc_c->span_h - cn;
    if (extra < 0) extra = 0;
    int top = 0;
    if      (rsc_c->valign == SC_VALIGN_MIDDLE) top = extra / 2;
    else if (rsc_c->valign == SC_VALIGN_BOTTOM) top = extra;
    return rsc_c->vis_offset + li - top;
}

/** Computes the content line index for a normal (non-rowspan) cell at visual
 *  line @p li, applying vertical alignment within the row height. */
static int compute_normal_cell_cli(int li, int cn, ScVAlign va, const RowRCtx *rctx) {
    int extra = rctx->row_h - cn - rctx->cpy;
    if (extra < 0) extra = 0;
    int top_pad = rctx->cpt;
    if      (va == SC_VALIGN_MIDDLE) top_pad += extra / 2;
    else if (va == SC_VALIGN_BOTTOM) top_pad += extra;
    return li - top_pad;
}

/** Returns @p so with header or footer text style applied to unstyled spans:
 *  only sets attr/fg when the span carries none of its own. */
static ScTextStyle apply_hdrftr_style(ScTextStyle so, const ScTableOpts *opts,
                                       const RowRCtx *rctx) {
    if (rctx->is_hdr) {
        if (so.attr == 0)      so.attr = opts->header.opts.attr;
        if (so.fg.index == -2) so.fg   = opts->header.opts.fg;
    } else if (rctx->is_ftr) {
        if (so.attr == 0)      so.attr = opts->footer.opts.attr;
        if (so.fg.index == -2) so.fg   = opts->footer.opts.fg;
    }
    return so;
}

/** Dispatches to print_cell_line_aligned() when content fits within @p cw,
 *  or print_cell_line_truncated() when it is wider. */
static void render_cell_line(const TLine *line, int cw, ScHAlign ha, ScColor cell_bg,
                              const ScTableOpts *opts, const RowRCtx *rctx) {
    int rw = cw - (int)line->vis_w;
    if (rw >= 0)
        print_cell_line_aligned(line, rw, ha, cell_bg, opts, rctx);
    else
        print_cell_line_truncated(line, cw, cell_bg, opts, rctx);
}

/** Prints all spans of @p line with left/right alignment padding totalling @p rw. */
static void print_cell_line_aligned(const TLine *line, int rw, ScHAlign ha,
                                     ScColor cell_bg, const ScTableOpts *opts,
                                     const RowRCtx *rctx) {
    int lp = 0, rp = rw;
    if      (ha == SC_ALIGN_CENTER) { lp = rw/2; rp = rw - lp; }
    else if (ha == SC_ALIGN_RIGHT)  { lp = rw;   rp = 0; }
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
            if (is_hcol_end && table->opts.border.header_col_sep_color.index != -2)
                sc_col = table->opts.border.header_col_sep_color;
            print_ch(tbc[bs].v, sc_col);
        }
    }
}

/* ── Section renderers ───────────────────────────────────────────────────── */

/* Renders the top border: title line if a top title is set, otherwise a plain
   horizontal border. No-op when no_outer is set. */
static void render_top_border(const Table *table) {
    ScBorderType bs = table->opts.border.style;
    ScColor oc = table->opts.border.outer_color;
    if (table->opts.border.no_outer) return;
    if (table->opts.title.text && table->opts.title.pos == SC_POSITION_TOP) {
        render_title_line(table, 1);
    } else if (bs != SC_BORDER_NONE) {
        render_horizontal_border(table, tbc[bs].tl, tbc[bs].tr,
                     tbc[bs].h, oc, tbc[bs].t_top, oc, oc, 0, NULL);
    }
}

/* Renders the header row (column names) followed by a separator line. */
static void render_header_row(const Table *table) {
    const ScTableData *table_data = table->table_data;
    if (!table->opts.header.row) return;
    ScBorderType bs  = table->opts.border.style;
    ScColor oc       = table->opts.border.outer_color;
    ScColor ic       = table->opts.border.inner_color;
    ScColor hrsc     = table->opts.border.header_row_sep_color;

    /* --- build synthetic header cells from column header strings --- */
    ScCell *hcells = malloc(table_data->column_count * sizeof(ScCell));
    for (size_t c = 0; c < table_data->column_count; c++) {
        hcells[c] = sc_cell(table_data->columns[c].header ? table_data->columns[c].header : "");
    }
    render_row(table, hcells, table->opts.header.row_bg, 1, 0);
    free(hcells);

    /* --- draw the separator below the header --- */
    if (bs != SC_BORDER_NONE) {
        ScColor hc = (hrsc.index != -2) ? hrsc : ic;
        render_horizontal_border(table, tbc[bs].t_left, tbc[bs].t_right,
                     tbc[bs].h, hc, tbc[bs].cross, hc, oc, 1, NULL);
    }
}

/* Renders all data rows with inner separators, rowspan tracking, and the
   optional truncation indicator when max_rows is set. */
static void render_data_rows(Table *table) {
    const ScTableData *table_data = table->table_data;
    ScBorderType bs = table->opts.border.style;
    int no_inner_h  = table->opts.border.no_inner_h;

    size_t max_r = table_data->row_count;
    if (table->opts.max_rows > 0 && (size_t)table->opts.max_rows < max_r)
        max_r = (size_t)table->opts.max_rows;

    for (size_t r = 0; r < max_r; r++) {
        /* --- initialize rowspan contexts for this row --- */
        for (size_t c = 0; c < table_data->column_count; c++) {
            int rs = table_data->rows[r].cells[c].rowspan;
            if (rs > 1) {
                /* compute total visual height of the span (content rows + separators) */
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

        /* --- draw inner separator before this row (skip first row) --- */
        if (r > 0 && bs != SC_BORDER_NONE && !no_inner_h) {
            for (size_t c = 0; c < table_data->column_count; c++)
                table->is_rs[c] = (table_data->rows[r].cells[c].rowspan == -1);
            render_inner_sep(table);
            /* advance vis_offset for spanning cells that crossed the separator */
            for (size_t c = 0; c < table_data->column_count; c++)
                if (table->is_rs[c] && table->rsc[c].cell) table->rsc[c].vis_offset++;
        }

        /* --- resolve row background (explicit > stripe > none) --- */
        ScColor row_bg = table_data->rows[r].bg;
        if (row_bg.index == -2) {
            row_bg = SC_ANSI_COLOR_NONE;
            if (table->opts.striped && (r % 2 == 1)) row_bg = table->opts.stripe_bg;
        }
        render_row(table, table_data->rows[r].cells, row_bg, 0, table->row_heights[r]);

        /* --- advance vis_offset for all active rowspan contexts --- */
        for (size_t c = 0; c < table_data->column_count; c++)
            if (table->rsc[c].cell) table->rsc[c].vis_offset += table->row_heights[r];
    }

    /* --- render truncation indicator when row count exceeds max_rows --- */
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

/* Renders the footer separator and all footer rows with inner separators. */
static void render_footer_rows(const Table *table) {
    const ScTableData *table_data = table->table_data;
    ScBorderType bs  = table->opts.border.style;
    ScColor oc       = table->opts.border.outer_color;
    ScColor ic       = table->opts.border.inner_color;
    ScColor hrsc     = table->opts.border.header_row_sep_color;

    /* --- separator between data rows and footer rows --- */
    if (table_data->footer_row_count > 0 && bs != SC_BORDER_NONE) {
        ScColor hc = (hrsc.index != -2) ? hrsc : ic;
        render_horizontal_border(table, tbc[bs].t_left, tbc[bs].t_right,
                     tbc[bs].h, hc, tbc[bs].cross, hc, oc, 1, NULL);
    }

    /* --- render each footer row, with inner separators between them --- */
    for (size_t r = 0; r < table_data->footer_row_count; r++) {
        if (r > 0 && bs != SC_BORDER_NONE && !table->opts.border.no_inner_h) {
            render_horizontal_border(table, tbc[bs].t_left, tbc[bs].t_right,
                         tbc[bs].h, ic, tbc[bs].cross, ic, oc, 1, NULL);
        }
        render_row(table, table_data->footer_rows[r].cells, table->opts.footer.row_bg, 2, 0);
    }
}

/* Renders the bottom border: title line if a bottom title is set, otherwise a
   plain horizontal border. No-op when no_outer is set. */
static void render_bottom_border(const Table *table) {
    ScBorderType bs = table->opts.border.style;
    ScColor oc = table->opts.border.outer_color;
    if (table->opts.border.no_outer) return;
    if (table->opts.title.text && table->opts.title.pos == SC_POSITION_BOTTOM) {
        render_title_line(table, 0);
    } else if (bs != SC_BORDER_NONE) {
        render_horizontal_border(table, tbc[bs].bl, tbc[bs].br,
                     tbc[bs].h, oc, tbc[bs].t_bot, oc, oc, 0, NULL);
    }
}
