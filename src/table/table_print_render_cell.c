#pragma once

#include "sparcli.h"
#include "internal.h"
#include "table_internal.h"
#include <stdlib.h>
#include <string.h>

/* Token type for the word-wrap subsystem: one contiguous run of spaces or
   non-spaces extracted from a TLine span. */
typedef struct { char *s; size_t vis_w; ScTextStyle opts; bool is_space; } WwTok;

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

// Forward declarations indented to reflect call hierarchy
static void   free_tlines  (TLine *lines, size_t n);
static void   flush_tline  (TLine **lines, size_t *cap, size_t *n, TSpan *buf, size_t buf_n, size_t vis_w);

static TLine *make_cell_lines      (const ScCell *cell, size_t *out_n);
    static void   resolve_cell_span    (const ScCell *cell, size_t si, const char **out_s, ScTextStyle *out_opts);
    static void   scan_text_for_newlines(const char *s, ScTextStyle opts, TLine **lines, size_t *lines_cap, size_t *nlines, TSpan **buf, size_t *buf_cap, size_t *buf_n, size_t *buf_w);
        static void   buf_push_segment (TSpan **buf, size_t *buf_cap, size_t *buf_n, size_t *buf_w, const char *start, size_t len, ScTextStyle opts);

static size_t cell_vis_width (const ScCell *cell);

static TLine *wrap_cell_lines      (const ScCell *cell, int wrap_w, size_t *out_n);
    static void   append_tline_copy    (TLine **res, size_t *nres, size_t *rcap, const TLine *src);
    static void   append_wrapped_lines (TLine **res, size_t *nres, size_t *rcap, const TLine *src, int wrap_w);
        static TLine *wrap_one_tline       (const TLine *src, int wrap_w, size_t *out_n);
            static void   tokenize_tline_spans (const TLine *src, WwTok **out_toks, size_t *out_n);
            static void   emit_space_tok       (WwAccum *a, WwTok *tok, const WwTok *next, int wrap_w);
                static void   ww_flush_sbuf    (WwAccum *a);
                static void   ww_push_span     (WwAccum *a, char *s, size_t vis_w, ScTextStyle opts);
            static void   emit_word_tok        (WwAccum *a, WwTok *tok, int wrap_w);
                static void   hard_break_word  (WwAccum *a, WwTok *tok, int wrap_w);


/* ── TLine memory helpers ────────────────────────────────────────────────── */

/* Frees all span strings inside each TLine, then the spans array and the
   lines array itself. */
static void free_tlines(TLine *lines, size_t n) {
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < lines[i].count; j++) {
            free((char *)lines[i].spans[j].text);
        }
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
    if (buf_n) { memcpy(ls, buf, buf_n * sizeof(TSpan)); }
    (*lines)[(*n)++] = (TLine){ ls, buf_n, vis_w };
}

/* ── Cell line building ───────────────────────────────────────────────────── */

/** Splits the content of @p cell on '\n' into an array of TLines. Each TLine
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

/** Scans @p s for '\n', calling buf_push_segment() for each text segment and
 *  flush_tline() on each newline. Remaining text after the last newline is
 *  left in the buffer for the caller to flush. */
static void scan_text_for_newlines(const char *s, ScTextStyle opts,
                                    TLine **lines, size_t *lines_cap, size_t *nlines,
                                    TSpan **buf, size_t *buf_cap, size_t *buf_n, size_t *buf_w) {
    const char *start = s;
    while (*s) {
        if (*s == '\n') {
            size_t len = (size_t)(s - start);
            if (len > 0) {
                buf_push_segment(buf, buf_cap, buf_n, buf_w, start, len, opts);
            }
            flush_tline(lines, lines_cap, nlines, *buf, *buf_n, *buf_w);
            *buf_n = 0; *buf_w = 0;
            start = s + 1;
        }
        s++;
    }
    size_t len = (size_t)(s - start);
    if (len > 0) {
        buf_push_segment(buf, buf_cap, buf_n, buf_w, start, len, opts);
    }
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
    for (size_t i = 0; i < n; i++) {
        if (lines[i].vis_w > max_w) { max_w = lines[i].vis_w; }
    }
    free_tlines(lines, n);
    return max_w;
}

/* ── Word-wrap ───────────────────────────────────────────────────────────── */

/** Wraps all lines of @p cell that exceed @p wrap_w columns, copying lines
 *  that fit via append_tline_copy() and wrapping wider ones via
 *  append_wrapped_lines(). */
static TLine *wrap_cell_lines(const ScCell *cell, int wrap_w, size_t *out_n) {
    size_t nraw;
    TLine *raw = make_cell_lines(cell, &nraw);
    if (wrap_w <= 0) { *out_n = nraw; return raw; }

    TLine *res = NULL; size_t nres = 0, rcap = 0;
    for (size_t li = 0; li < nraw; li++) {
        if ((int)raw[li].vis_w <= wrap_w) {
            append_tline_copy(&res, &nres, &rcap, &raw[li]);
        } else {
            append_wrapped_lines(&res, &nres, &rcap, &raw[li], wrap_w);
        }
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
    for (size_t j = 0; j < src->count; j++) {
        ls[j] = (TSpan){ strdup(src->spans[j].text), src->spans[j].opts };
    }
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
        while (*rcap < *nres + nw) { *rcap = *rcap ? *rcap*2 : 4; }
        *res = realloc(*res, *rcap * sizeof(TLine));
    }
    memcpy(*res + *nres, wrapped, nw * sizeof(TLine));
    free(wrapped);
    *nres += nw;
}

/** Wraps a single TLine into multiple TLines each fitting within @p wrap_w cols.
 *  Tokenizes the line, dispatches each token to emit_space_tok() or
 *  emit_word_tok(), then flushes the final accumulated line. */
static TLine *wrap_one_tline(const TLine *src, int wrap_w, size_t *out_n) {
    WwTok *toks; size_t ntok;
    tokenize_tline_spans(src, &toks, &ntok);

    WwAccum acc = {0};
    for (size_t ti = 0; ti < ntok; ti++) {
        const WwTok *next = (ti + 1 < ntok) ? &toks[ti + 1] : NULL;
        if (toks[ti].is_space) {
            emit_space_tok(&acc, &toks[ti], next, wrap_w);
        } else {
            emit_word_tok(&acc, &toks[ti], wrap_w);
        }
    }
    ww_flush_sbuf(&acc);

    TLine *res = acc.res;
    free(acc.sbuf);
    for (size_t ti = 0; ti < ntok; ti++) { if (toks[ti].s) { free(toks[ti].s); } }
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
            bool is_sp = (*p == ' ');
            while (*p && ((*p == ' ') == is_sp)) { p++; }
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

/** Emits a word token: flushes the current line first if the word doesn't fit,
 *  then hard-breaks it when it exceeds @p wrap_w, otherwise appends it. */
static void emit_word_tok(WwAccum *a, WwTok *tok, int wrap_w) {
    if (a->sn > 0 && (int)(a->sw + tok->vis_w) > wrap_w) { ww_flush_sbuf(a); }
    if ((int)tok->vis_w > wrap_w) {
        hard_break_word(a, tok, wrap_w);
    } else {
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
        if (cb == 0) { break; }
        size_t cw2 = sc_utf8_string_length(p, cb);
        ww_push_span(a, strndup(p, cb), cw2, tok->opts);
        p += cb; rem -= cb;
        if (rem > 0) { ww_flush_sbuf(a); }
    }
    free(tok->s); tok->s = NULL;
}
