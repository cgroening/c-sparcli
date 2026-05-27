#pragma once

#include "sparcli.h"
#include "internal.h"
#include "table_internal.h"
#include <stdlib.h>
#include <string.h>

/* Token type for the word-wrap subsystem: one contiguous run of spaces or
   non-spaces extracted from a TLine span. */
typedef struct { char *s; size_t vis_w; ScTextStyle opts; bool is_space; } WordWrapToken;

/* Mutable accumulator for the word-wrap line-building loop. */
typedef struct {
    TLine  *lines;         /* completed output lines */
    size_t  line_count;
    size_t  line_capacity;
    TSpan  *spans;         /* spans accumulating for the current output line */
    size_t  span_count;
    size_t  span_capacity;
    size_t  span_width;    /* visible width of the current output line */
} WordWrapAccum;

// Forward declarations indented to reflect call hierarchy
static void   free_tlines  (TLine *lines, size_t line_count);
static void   flush_tline  (TLine **lines, size_t *line_cap, size_t *line_count, TSpan *buf, size_t buf_count, size_t vis_w);

static TLine *make_cell_lines      (const ScCell *cell, size_t *out_n);
    static void   resolve_cell_span    (const ScCell *cell, size_t span_idx, const char **out_s, ScTextStyle *out_opts);
    static void   scan_text_for_newlines(const char *text, ScTextStyle opts, TLine **lines, size_t *line_cap, size_t *line_count, TSpan **buf, size_t *buf_cap, size_t *buf_count, size_t *buf_width);
        static void   buf_push_segment (TSpan **buf, size_t *buf_cap, size_t *buf_count, size_t *buf_width, const char *start, size_t len, ScTextStyle opts);

static size_t cell_vis_width (const ScCell *cell);

static TLine *wrap_cell_lines      (const ScCell *cell, int wrap_w, size_t *out_n);
    static void   append_tline_copy    (TLine **out_lines, size_t *out_count, size_t *out_cap, const TLine *src);
    static void   append_wrapped_lines (TLine **out_lines, size_t *out_count, size_t *out_cap, const TLine *src, int wrap_w);
        static TLine *wrap_one_tline       (const TLine *src, int wrap_w, size_t *out_n);
            static void   tokenize_tline_spans (const TLine *src, WordWrapToken **out_tokens, size_t *out_n);
            static void   emit_space_tok       (WordWrapAccum *state, WordWrapToken *tok, const WordWrapToken *next, int wrap_w);
                static void   ww_flush_sbuf    (WordWrapAccum *state);
                static void   ww_push_span     (WordWrapAccum *state, char *text, size_t vis_w, ScTextStyle opts);
            static void   emit_word_tok        (WordWrapAccum *state, WordWrapToken *tok, int wrap_w);
                static void   hard_break_word  (WordWrapAccum *state, WordWrapToken *tok, int wrap_w);


/* ── TLine memory helpers ────────────────────────────────────────────────── */

/* Frees all span strings inside each TLine, then the spans array and the
   lines array itself. */
static void free_tlines(TLine *lines, size_t line_count) {
    for (size_t line_idx = 0; line_idx < line_count; line_idx++) {
        for (size_t span_idx = 0; span_idx < lines[line_idx].count; span_idx++) {
            free((char *)lines[line_idx].spans[span_idx].text);
        }
        free(lines[line_idx].spans);
    }
    free(lines);
}

/* Copies the span buffer into a new TLine entry and appends it to the lines
   array, growing the array if needed. */
static void flush_tline(TLine **lines, size_t *line_cap, size_t *line_count,
                         TSpan *buf, size_t buf_count, size_t vis_w) {
    /* --- grow the lines array if full --- */
    if (*line_count == *line_cap) {
        *line_cap = *line_cap ? *line_cap * 2 : 4;
        *lines = realloc(*lines, *line_cap * sizeof(TLine));
    }
    /* --- copy the span buffer into a new owned TLine --- */
    TSpan *span_copy = malloc((buf_count + 1) * sizeof(TSpan));
    if (buf_count) { memcpy(span_copy, buf, buf_count * sizeof(TSpan)); }
    (*lines)[(*line_count)++] = (TLine){ span_copy, buf_count, vis_w };
}

/* ── Cell line building ───────────────────────────────────────────────────── */

/** Splits the content of @p cell on '\n' into an array of TLines. Each TLine
 *  owns heap copies of its span strings. Delegates span resolution and newline
 *  scanning to resolve_cell_span() and scan_text_for_newlines(). */
static TLine *make_cell_lines(const ScCell *cell, size_t *out_n) {
    size_t line_cap = 4, line_count = 0;
    TLine *lines = malloc(line_cap * sizeof(TLine));
    size_t buf_cap = 4, buf_count = 0, buf_width = 0;
    TSpan *buf = malloc(buf_cap * sizeof(TSpan));

    size_t span_count = ((cell->kind == SC_CELL_TEXT || cell->kind == SC_CELL_MARKUP) && cell->text)
        ? cell->text->count : 1;

    for (size_t span_idx = 0; span_idx < span_count; span_idx++) {
        const char *span_text;
        ScTextStyle span_style;
        resolve_cell_span(cell, span_idx, &span_text, &span_style);
        scan_text_for_newlines(
            span_text, span_style,
            &lines, &line_cap, &line_count,
            &buf, &buf_cap, &buf_count, &buf_width
        );
    }

    flush_tline(&lines, &line_cap, &line_count, buf, buf_count, buf_width);
    free(buf);
    *out_n = line_count;
    return lines;
}

/** Resolves the raw string and text style for span index @p si of @p cell.
 *  SC_CELL_STR cells have exactly one span and use a plain (unstyled) style. */
static void resolve_cell_span(const ScCell *cell, size_t span_idx,
                               const char **out_s, ScTextStyle *out_opts) {
    static const ScTextStyle PLAIN = { 0, { -2,0,0,0 }, { -2,0,0,0 } };
    if (cell->kind == SC_CELL_STR) {
        *out_s    = cell->str ? cell->str : "";
        *out_opts = PLAIN;
    } else {
        *out_s    = cell->text->spans[span_idx].raw_str;
        *out_opts = cell->text->spans[span_idx].style;
    }
}

/** Scans @p s for '\n', calling buf_push_segment() for each text segment and
 *  flush_tline() on each newline. Remaining text after the last newline is
 *  left in the buffer for the caller to flush. */
static void scan_text_for_newlines(const char *text, ScTextStyle opts,
                                    TLine **lines, size_t *line_cap, size_t *line_count,
                                    TSpan **buf, size_t *buf_cap, size_t *buf_count, size_t *buf_width) {
    const char *start = text;
    while (*text) {
        if (*text == '\n') {
            size_t len = (size_t)(text - start);
            if (len > 0) {
                buf_push_segment(buf, buf_cap, buf_count, buf_width, start, len, opts);
            }
            flush_tline(lines, line_cap, line_count, *buf, *buf_count, *buf_width);
            *buf_count = 0; *buf_width = 0;
            start = text + 1;
        }
        text++;
    }
    size_t len = (size_t)(text - start);
    if (len > 0) {
        buf_push_segment(buf, buf_cap, buf_count, buf_width, start, len, opts);
    }
}

/** Appends the text segment [@p start, @p start+@p len) with style @p opts to
 *  the span buffer, doubling its capacity if it is full. */
static void buf_push_segment(TSpan **buf, size_t *buf_cap, size_t *buf_count, size_t *buf_width,
                              const char *start, size_t len, ScTextStyle opts) {
    if (*buf_count == *buf_cap) {
        *buf_cap *= 2;
        *buf = realloc(*buf, *buf_cap * sizeof(TSpan));
    }
    (*buf)[(*buf_count)++] = (TSpan){ strndup(start, len), opts };
    *buf_width += sc_utf8_string_length(start, len);
}

/* Returns the visible column width of the widest line in a cell. */
static size_t cell_vis_width(const ScCell *cell) {
    size_t line_count;
    TLine *lines = make_cell_lines(cell, &line_count);
    size_t max_width = 0;
    for (size_t i = 0; i < line_count; i++) {
        if (lines[i].vis_w > max_width) { max_width = lines[i].vis_w; }
    }
    free_tlines(lines, line_count);
    return max_width;
}

/* ── Word-wrap ───────────────────────────────────────────────────────────── */

/** Wraps all lines of @p cell that exceed @p wrap_w columns, copying lines
 *  that fit via append_tline_copy() and wrapping wider ones via
 *  append_wrapped_lines(). */
static TLine *wrap_cell_lines(const ScCell *cell, int wrap_w, size_t *out_n) {
    size_t raw_count;
    TLine *raw_lines = make_cell_lines(cell, &raw_count);
    if (wrap_w <= 0) { *out_n = raw_count; return raw_lines; }

    TLine *result_lines = NULL; size_t result_count = 0, result_cap = 0;
    for (size_t i = 0; i < raw_count; i++) {
        if ((int)raw_lines[i].vis_w <= wrap_w) {
            append_tline_copy(&result_lines, &result_count, &result_cap, &raw_lines[i]);
        } else {
            append_wrapped_lines(&result_lines, &result_count, &result_cap, &raw_lines[i], wrap_w);
        }
    }
    free_tlines(raw_lines, raw_count);
    *out_n = result_count;
    return result_lines;
}

/** Deep-copies @p src's spans into a new TLine and appends it to @p res,
 *  growing the array if needed. */
static void append_tline_copy(TLine **out_lines, size_t *out_count, size_t *out_cap, const TLine *src) {
    if (*out_count == *out_cap) {
        *out_cap = *out_cap ? *out_cap * 2 : 4;
        *out_lines = realloc(*out_lines, *out_cap * sizeof(TLine));
    }
    TSpan *span_copy = malloc((src->count + 1) * sizeof(TSpan));
    for (size_t i = 0; i < src->count; i++) {
        span_copy[i] = (TSpan){ strdup(src->spans[i].text), src->spans[i].opts };
    }
    (*out_lines)[(*out_count)++] = (TLine){ span_copy, src->count, src->vis_w };
}

/** Wraps @p src via wrap_one_tline() and bulk-appends all resulting TLines into
 *  @p res. The temporary wrap buffer is freed shallowly — span ownership
 *  transfers to @p res. */
static void append_wrapped_lines(TLine **out_lines, size_t *out_count, size_t *out_cap,
                                  const TLine *src, int wrap_w) {
    size_t wrapped_count;
    TLine *wrapped_lines = wrap_one_tline(src, wrap_w, &wrapped_count);
    if (*out_count + wrapped_count > *out_cap) {
        while (*out_cap < *out_count + wrapped_count) {
            *out_cap = *out_cap ? *out_cap * 2 : 4;
        }
        *out_lines = realloc(*out_lines, *out_cap * sizeof(TLine));
    }
    memcpy(*out_lines + *out_count, wrapped_lines, wrapped_count * sizeof(TLine));
    free(wrapped_lines);
    *out_count += wrapped_count;
}

/** Wraps a single TLine into multiple TLines each fitting within @p wrap_w cols.
 *  Tokenizes the line, dispatches each token to emit_space_tok() or
 *  emit_word_tok(), then flushes the final accumulated line. */
static TLine *wrap_one_tline(const TLine *src, int wrap_w, size_t *out_n) {
    WordWrapToken *tokens; size_t token_count;
    tokenize_tline_spans(src, &tokens, &token_count);

    WordWrapAccum state = {0};
    for (size_t i = 0; i < token_count; i++) {
        const WordWrapToken *next_token = (i + 1 < token_count) ? &tokens[i + 1] : NULL;
        if (tokens[i].is_space) {
            emit_space_tok(&state, &tokens[i], next_token, wrap_w);
        } else {
            emit_word_tok(&state, &tokens[i], wrap_w);
        }
    }
    ww_flush_sbuf(&state);

    TLine *result_lines = state.lines;
    free(state.spans);
    for (size_t i = 0; i < token_count; i++) { if (tokens[i].s) { free(tokens[i].s); } }
    free(tokens);
    *out_n = state.line_count;
    return result_lines;
}

/** Splits each span of @p src into alternating non-space/space-run WwTok tokens.
 *  The caller owns the returned array and must free each token's @c s field. */
static void tokenize_tline_spans(const TLine *src, WordWrapToken **out_tokens, size_t *out_n) {
    WordWrapToken *tokens = NULL; size_t token_count = 0, token_cap = 0;
    for (size_t span_idx = 0; span_idx < src->count; span_idx++) {
        const char *text_ptr = src->spans[span_idx].text;
        ScTextStyle span_style = src->spans[span_idx].opts;
        while (*text_ptr) {
            const char *run_start = text_ptr;
            bool is_space_run = (*text_ptr == ' ');
            while (*text_ptr && ((*text_ptr == ' ') == is_space_run)) { text_ptr++; }
            size_t run_len = (size_t)(text_ptr - run_start);
            size_t run_vis_width = is_space_run ? run_len : sc_utf8_string_length(run_start, run_len);
            if (token_count == token_cap) {
                token_cap = token_cap ? token_cap * 2 : 16;
                tokens = realloc(tokens, token_cap * sizeof(WordWrapToken));
            }
            tokens[token_count++] = (WordWrapToken){ strndup(run_start, run_len), run_vis_width, span_style, is_space_run };
        }
    }
    *out_tokens = tokens;
    *out_n      = token_count;
}

/** Emits a space token: drops it when at line start, flushes and wraps when
 *  the following word won't fit after it, otherwise appends it to the line. */
static void emit_space_tok(WordWrapAccum *state, WordWrapToken *tok, const WordWrapToken *next, int wrap_w) {
    if (state->span_count == 0) { free(tok->s); tok->s = NULL; return; }
    if (next && !next->is_space
            && (int)(state->span_width + tok->vis_w + next->vis_w) > wrap_w) {
        free(tok->s); tok->s = NULL;
        ww_flush_sbuf(state);
        return;
    }
    ww_push_span(state, tok->s, tok->vis_w, tok->opts);
    tok->s = NULL;
}

/** Trims trailing space spans from the current line, flushes it as a new TLine
 *  into @p a->res via flush_tline(), then resets the span count and width. */
static void ww_flush_sbuf(WordWrapAccum *state) {
    while (state->span_count > 0 && state->spans[state->span_count - 1].text[0] == ' ') {
        state->span_width -= strlen(state->spans[state->span_count - 1].text);
        free((char *)state->spans[--state->span_count].text);
    }
    flush_tline(
        &state->lines, &state->line_capacity, &state->line_count,
        state->spans, state->span_count, state->span_width
    );
    state->span_count = 0; state->span_width = 0;
}

/** Appends span @p s (taking ownership) to the current-line accumulator,
 *  growing the span buffer if needed, and adds @p vis_w to the line width. */
static void ww_push_span(WordWrapAccum *state, char *text, size_t vis_w, ScTextStyle opts) {
    if (state->span_count == state->span_capacity) {
        state->span_capacity = state->span_capacity ? state->span_capacity * 2 : 8;
        state->spans = realloc(state->spans, state->span_capacity * sizeof(TSpan));
    }
    state->spans[state->span_count++] = (TSpan){ text, opts };
    state->span_width += vis_w;
}

/** Emits a word token: flushes the current line first if the word doesn't fit,
 *  then hard-breaks it when it exceeds @p wrap_w, otherwise appends it. */
static void emit_word_tok(WordWrapAccum *state, WordWrapToken *tok, int wrap_w) {
    if (state->span_count > 0 && (int)(state->span_width + tok->vis_w) > wrap_w) {
        ww_flush_sbuf(state);
    }
    if ((int)tok->vis_w > wrap_w) {
        hard_break_word(state, tok, wrap_w);
    } else {
        ww_push_span(state, tok->s, tok->vis_w, tok->opts);
        tok->s = NULL;
    }
}

/** Splits a word token wider than @p wrap_w into fit-sized chunks, flushing
 *  a new output line after each full chunk. Frees @p tok->s when done. */
static void hard_break_word(WordWrapAccum *state, WordWrapToken *tok, int wrap_w) {
    const char *text_ptr = tok->s;
    size_t remaining_bytes = strlen(tok->s);
    while (remaining_bytes > 0) {
        int avail_cols = wrap_w - (int)state->span_width;
        if (avail_cols <= 0) { ww_flush_sbuf(state); avail_cols = wrap_w; }
        size_t chunk_bytes = sc_utf8_trim_to_cols(text_ptr, avail_cols);
        if (chunk_bytes == 0) { break; }
        size_t chunk_vis_width = sc_utf8_string_length(text_ptr, chunk_bytes);
        ww_push_span(state, strndup(text_ptr, chunk_bytes), chunk_vis_width, tok->opts);
        text_ptr += chunk_bytes; remaining_bytes -= chunk_bytes;
        if (remaining_bytes > 0) { ww_flush_sbuf(state); }
    }
    free(tok->s); tok->s = NULL;
}
