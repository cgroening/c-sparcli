#pragma once

#include "sparcli.h"
#include "internal.h"
#include "table_internal.h"
#include <stdlib.h>
#include <string.h>

/**
 * Token type for the word-wrap subsystem: one contiguous run of spaces or
 * non-spaces extracted from a `TLine` span.
 */
typedef struct {
    char *string;
    size_t visible_width;
    ScTextStyle opts;
    bool is_space;
} WordWrapToken;

/** Mutable accumulator for the word-wrap line-building loop. */
typedef struct WordWrapAccum {
    /** Completed output lines. */
    TLine  *lines;

    /** Number of completed lines in `lines`. */
    size_t  line_count;

    /** Allocated slot count in `lines`. */
    size_t  line_capacity;

    /** Spans accumulating for the current output line. */
    TSpan  *spans;

    /** Number of spans in the current line buffer. */
    size_t  span_count;

    /** Allocated slot count in `spans`. */
    size_t  span_capacity;

    /** Visible column width of the current output line. */
    size_t  span_width;
} WordWrapAccum;

/**
 * Arguments for `flush_tline(
 )`:
 * the destination line array and the line data to append.
 */
typedef struct TLineFlush {
    /** Pointer to the growing output array. */
    TLine  **out_lines;

    /** Allocated slot count in `*out_lines`. */
    size_t  *out_capacity;

    /** Number of completed lines written into `*out_lines`. */
    size_t  *out_count;

    /** Span buffer for the line being appended. */
    TSpan   *spans;

    /** Number of spans in `spans`. */
    size_t   span_count;

    /** Visible column width of the line being appended. */
    size_t   visible_width;
} TLineFlush;

/**
 * Working state shared by `scan_text_for_newlines()` and `buf_push_segment()`.
 */
typedef struct LineScanCtx {
    /** Pointer to the growing output lines array. */
    TLine  **lines;

    /** Number of completed lines in `*lines`. */
    size_t  *line_count;

    /** Allocated slot count in `*lines`. */
    size_t  *line_capacity;

    /** Pointer to the growing span buffer for the current line. */
    TSpan  **spans;

    /** Number of spans currently in `*spans`. */
    size_t  *span_count;

    /** Allocated slot count in `*spans`. */
    size_t  *span_capacity;

    /** Visible column width of the current line. */
    size_t  *span_width;
} LineScanCtx;


/**
 * Growable output buffer for `TLine` rows, shared by `append_tline_copy()`
 * and `append_wrapped_lines()`.
 */
typedef struct {
    /** Pointer to the growing line array. */
    TLine  **lines;

    /** Number of lines in `*lines`. */
    size_t  *count;

    /** Allocated slot count in `*lines`. */
    size_t  *capacity;
} TLineBuf;


// Forward declarations indented to reflect call hierarchy

static TLine *make_cell_lines(const ScCell *cell, size_t *out_count);
    static void resolve_cell_span(
        const ScCell *cell,
        size_t span_idx,
        const char **out_string,
        ScTextStyle *out_opts
    );
    static void scan_text_for_newlines(
        const char *text, ScTextStyle opts, LineScanCtx *ctx
    );
        static void buf_push_segment(
            LineScanCtx *ctx, const char *start, size_t len, ScTextStyle opts
        );
    static void flush_tline(TLineFlush args);

static size_t cell_vis_width(const ScCell *cell);
    static void free_tlines(TLine *lines, size_t line_count);

static TLine *wrap_cell_lines(const ScCell *cell, int wrap_wrap, size_t *out_n);
    static void append_tline_copy(TLineBuf *result, const TLine *line);
    static void append_wrapped_lines(
        TLineBuf *result, const TLine *line, int wrap_word
    );
        static TLine *wrap_one_tline(
            const TLine *line, int wrap_word, size_t *out_n
        );
            static void tokenize_tline_spans(
                const TLine *line, WordWrapToken **out_tokens, size_t *out_n
            );
            static void emit_space_tok(
                WordWrapAccum *state,
                WordWrapToken *tok,
                const WordWrapToken *next,
                int wrap_word
            );
                static void ww_flush_sbuf(WordWrapAccum *state);
                static void ww_push_span(
                    WordWrapAccum *state,
                    char *text,
                    size_t visible_width,
                    ScTextStyle opts
                );
            static void emit_word_tok(
                WordWrapAccum *state, WordWrapToken *tok, int wrap_word
            );
                static void hard_break_word(
                    WordWrapAccum *state, WordWrapToken *tok, int wrap_word
                );


/**
 * Splits the content of `cell` on '\n' into an array of TLines. Each TLine
 * owns heap copies of its span strings. Delegates span resolution and newline
 * scanning to `resolve_cell_span()` and `scan_text_for_newlines()`.
 */
static TLine *make_cell_lines(const ScCell *cell, size_t *out_n) {
    size_t line_capacity = 4, line_count = 0;
    TLine *lines = malloc(line_capacity * sizeof(TLine));
    size_t span_capacity = 4, span_count = 0, span_width = 0;
    TSpan *spans = malloc(span_capacity * sizeof(TSpan));
    LineScanCtx ctx = {
        .lines         = &lines,
        .line_count    = &line_count,
        .line_capacity = &line_capacity,
        .spans         = &spans,
        .span_count    = &span_count,
        .span_capacity = &span_capacity,
        .span_width    = &span_width,
    };

    // Text/markup cells carry one `ScSpan` per styled segment;
    // all other kinds have exactly one.
    size_t num_spans;
    if (
        (cell->kind == SC_CELL_TEXT || cell->kind == SC_CELL_MARKUP) &&
        cell->text
    ) {
        num_spans = cell->text->count;
    } else {
        num_spans = 1;
    }

    for (size_t span_idx = 0; span_idx < num_spans; span_idx++) {
        const char *span_text;
        ScTextStyle span_style;
        resolve_cell_span(cell, span_idx, &span_text, &span_style);
        scan_text_for_newlines(span_text, span_style, &ctx);
    }

    flush_tline((TLineFlush){
        .out_lines    = &lines,
        .out_capacity = &line_capacity,
        .out_count    = &line_count,
        .spans        = spans,
        .span_count   = span_count,
        .visible_width = span_width,
    });
    free(spans);
    *out_n = line_count;
    return lines;
}

/**
 * Resolves the raw string and text style for span index `span_idx` of `cell`.
 * `SC_CELL_STR` cells have exactly one span and use a plain (unstyled) style.
 */
static void resolve_cell_span(
    const ScCell *cell,
    size_t span_idx,
    const char **out_string,
    ScTextStyle *out_opts
) {
    static const ScTextStyle PLAIN = { 0, { -2,0,0,0 }, { -2,0,0,0 } };
    if (cell->kind == SC_CELL_STR) {
        *out_string = cell->str ? cell->str : "";
        *out_opts = PLAIN;
    } else {
        *out_string = cell->text->spans[span_idx].raw_str;
        *out_opts = cell->text->spans[span_idx].style;
    }
}

/**
 * Scans `text` for '\n', calling `buf_push_segment()` for each text segment
 * and `flush_tline()` on each newline. Remaining text after the last newline
 * is left in the buffer for the caller to flush.
 */
static void scan_text_for_newlines(
    const char *text, ScTextStyle opts, LineScanCtx *ctx
) {
    const char *start = text;
    while (*text) {
        if (*text == '\n') {
            size_t len = (size_t)(text - start);
            if (len > 0) {
                buf_push_segment(ctx, start, len, opts);
            }
            flush_tline((TLineFlush){
                .out_lines    = ctx->lines,
                .out_capacity = ctx->line_capacity,
                .out_count    = ctx->line_count,
                .spans        = *ctx->spans,
                .span_count   = *ctx->span_count,
                .visible_width = *ctx->span_width,
            });
            *ctx->span_count = 0; *ctx->span_width = 0;
            start = text + 1;
        }
        text++;
    }
    size_t len = (size_t)(text - start);
    if (len > 0) {
        buf_push_segment(ctx, start, len, opts);
    }
}

/**
 * Appends the text segment [`start`, `start`+`len`) with style `opts` to
 * the span buffer, doubling its capacity if it is full.
 */
static void buf_push_segment(
    LineScanCtx *ctx, const char *start, size_t len, ScTextStyle opts
) {
    if (*ctx->span_count == *ctx->span_capacity) {
        *ctx->span_capacity *= 2;
        *ctx->spans = realloc(*ctx->spans, *ctx->span_capacity * sizeof(TSpan));
    }
    (*ctx->spans)[(*ctx->span_count)++] = (TSpan){
        .text = strndup(start, len),
        .opts = opts
    };
    *ctx->span_width += sc_utf8_string_length(start, len);
}

/**
 * Copies the span buffer into a new TLine entry and appends it to the lines
 * array, growing the array if needed.
 */
static void flush_tline(TLineFlush args) {
    if (*args.out_count == *args.out_capacity) {
        *args.out_capacity = *args.out_capacity ? *args.out_capacity * 2 : 4;
        *args.out_lines = realloc(
            *args.out_lines, *args.out_capacity * sizeof(TLine)
        );
    }
    TSpan *span_copy = malloc((args.span_count + 1) * sizeof(TSpan));
    if (args.span_count) {
        memcpy(span_copy, args.spans, args.span_count * sizeof(TSpan));
    }
    (*args.out_lines)[(*args.out_count)++] = (TLine){
        .spans = span_copy,
        .count = args.span_count,
        .visible_width = args.visible_width
    };
}

/** Returns the visible column width of the widest line in a cell. */
static size_t cell_vis_width(const ScCell *cell) {
    size_t line_count;
    TLine *lines = make_cell_lines(cell, &line_count);
    size_t max_width = 0;
    for (size_t i = 0; i < line_count; i++) {
        if (lines[i].visible_width > max_width) {
            max_width = lines[i].visible_width;
        }
    }
    free_tlines(lines, line_count);
    return max_width;
}

/**
 * Frees all span strings inside each TLine, then the spans array and the
 * lines array itself.
 */
static void free_tlines(TLine *lines, size_t line_count) {
    for (size_t line_idx = 0; line_idx < line_count; line_idx++) {
        for (size_t span_idx = 0; span_idx < lines[line_idx].count; span_idx++) {
            free((char *)lines[line_idx].spans[span_idx].text);
        }
        free(lines[line_idx].spans);
    }
    free(lines);
}

/**
 * Wraps all lines of `cell` that exceed `wrap_w` columns, copying lines that
 * fit via `append_tline_copy()` and wrapping wider ones via
 * `append_wrapped_lines()`.
 */
static TLine *wrap_cell_lines(
    const ScCell *cell, int wrap_word, size_t *out_count
) {
    size_t raw_count;
    TLine *raw_lines = make_cell_lines(cell, &raw_count);
    if (wrap_word <= 0) { *out_count = raw_count; return raw_lines; }

    TLine *result_lines = NULL; size_t result_count = 0, result_capacity = 0;
    TLineBuf out = {
        .lines    = &result_lines,
        .count    = &result_count,
        .capacity = &result_capacity,
    };
    for (size_t i = 0; i < raw_count; i++) {
        if ((int)raw_lines[i].visible_width <= wrap_word) {
            append_tline_copy(&out, &raw_lines[i]);
        } else {
            append_wrapped_lines(&out, &raw_lines[i], wrap_word);
        }
    }
    free_tlines(raw_lines, raw_count);
    *out_count = result_count;
    return result_lines;
}

/**
 * Deep-copies `line`'s spans into a new TLine and appends it to `result`,
 * growing the array if needed.
 */
static void append_tline_copy(TLineBuf *result, const TLine *line) {
    if (*result->count == *result->capacity) {
        *result->capacity = *result->capacity ? *result->capacity * 2 : 4;
        *result->lines = realloc(
            *result->lines, *result->capacity * sizeof(TLine)
        );
    }
    TSpan *span_copy = malloc((line->count + 1) * sizeof(TSpan));
    for (size_t i = 0; i < line->count; i++) {
        span_copy[i] = (TSpan){
            .text = strdup(line->spans[i].text),
            .opts = line->spans[i].opts
        };
    }
    (*result->lines)[(*result->count)++] = (TLine){
        .spans = span_copy,
        .count = line->count,
        .visible_width = line->visible_width
    };
}

/**
 * Wraps `line` via `wrap_one_tline()` and bulk-appends all resulting TLines
 * into `result`. The temporary wrap buffer is freed shallowly — span
 * ownership transfers to `result`.
 */
static void append_wrapped_lines(
    TLineBuf *result, const TLine *line, int wrap_word
) {
    size_t wrapped_count;
    TLine *wrapped_lines = wrap_one_tline(line, wrap_word, &wrapped_count);
    if (*result->count + wrapped_count > *result->capacity) {
        while (*result->capacity < *result->count + wrapped_count) {
            *result->capacity = *result->capacity ? *result->capacity * 2 : 4;
        }
        *result->lines = realloc(
            *result->lines, *result->capacity * sizeof(TLine)
        );
    }
    memcpy(
        *result->lines + *result->count,
        wrapped_lines,
        wrapped_count * sizeof(TLine
    ));
    free(wrapped_lines);
    *result->count += wrapped_count;
}

/**
 * Wraps a single TLine into multiple TLines each fitting within `wrap_w`
 * columns. Tokenizes the line, dispatches each token to `emit_space_tok()`
 * or `emit_word_tok()`, then flushes the final accumulated line.
 */
static TLine *wrap_one_tline(const TLine *line, int wrap_w, size_t *out_n) {
    WordWrapToken *tokens; size_t token_count;
    tokenize_tline_spans(line, &tokens, &token_count);

    WordWrapAccum state = {0};
    for (size_t i = 0; i < token_count; i++) {
        const WordWrapToken *next_token =
            (i + 1 < token_count) ? &tokens[i + 1] : NULL;
        if (tokens[i].is_space) {
            emit_space_tok(&state, &tokens[i], next_token, wrap_w);
        } else {
            emit_word_tok(&state, &tokens[i], wrap_w);
        }
    }
    ww_flush_sbuf(&state);

    TLine *result_lines = state.lines;
    free(state.spans);
    for (size_t i = 0; i < token_count; i++) {
        if (tokens[i].string) {
            free(tokens[i].string);
        }
    }
    free(tokens);
    *out_n = state.line_count;
    return result_lines;
}

/**
 * Splits each span of `line` into alternating space-run and word-run
 * `WordWrapToken` values. The caller owns the returned array and must free
 * each token's `string` field.
 */
static void tokenize_tline_spans(
    const TLine *line, WordWrapToken **out_tokens, size_t *out_count
) {
    WordWrapToken *tokens = NULL; size_t token_count = 0, token_cap = 0;
    for (size_t span_idx = 0; span_idx < line->count; span_idx++) {
        const char *text_ptr = line->spans[span_idx].text;
        ScTextStyle span_style = line->spans[span_idx].opts;
        while (*text_ptr) {
            const char *run_start = text_ptr;
            bool is_space_run = (*text_ptr == ' ');
            while (*text_ptr && ((*text_ptr == ' ') == is_space_run)) {
                text_ptr++;
            }
            size_t run_len = (size_t)(text_ptr - run_start);
            size_t run_vis_width = is_space_run ?
                run_len : sc_utf8_string_length(run_start, run_len);
            if (token_count == token_cap) {
                token_cap = token_cap ? token_cap * 2 : 16;
                tokens = realloc(tokens, token_cap * sizeof(WordWrapToken));
            }
            tokens[token_count++] = (WordWrapToken){
                .string        = strndup(run_start, run_len),
                .visible_width = run_vis_width,
                .opts          = span_style,
                .is_space      = is_space_run,
            };
        }
    }
    *out_tokens = tokens;
    *out_count      = token_count;
}

/**
 * Emits a space token: drops it when at line start, flushes and wraps when
 * the following word won't fit after it, otherwise appends it to the line.
 */
static void emit_space_tok(
    WordWrapAccum *state,
    WordWrapToken *tok,
    const WordWrapToken *next,
    int wrap_word
) {
    if (state->span_count == 0) {
        free(tok->string); tok->string = NULL; return;
    }
    if (
        next && !next->is_space &&
        (int)(state->span_width + tok->visible_width + next->visible_width)
            > wrap_word
    ) {
        free(tok->string); tok->string = NULL;
        ww_flush_sbuf(state);
        return;
    }
    ww_push_span(state, tok->string, tok->visible_width, tok->opts);
    tok->string = NULL;
}

/**
 * Trims trailing space spans from the current line, flushes it as a new TLine
 * into `state->lines` via `flush_tline()`, then resets `span_count` and
 * `span_width`.
 */
static void ww_flush_sbuf(WordWrapAccum *state) {
    while (
        state->span_count > 0 &&
        state->spans[state->span_count - 1].text[0] == ' '
    ) {
        state->span_width -= strlen(state->spans[state->span_count - 1].text);
        free((char *)state->spans[--state->span_count].text);
    }
    flush_tline((TLineFlush){
        .out_lines    = &state->lines,
        .out_capacity = &state->line_capacity,
        .out_count    = &state->line_count,
        .spans        = state->spans,
        .span_count   = state->span_count,
        .visible_width = state->span_width,
    });
    state->span_count = 0; state->span_width = 0;
}

/**
 * Appends `text` (taking ownership) to the current-line accumulator,
 * growing the span buffer if needed, and adds `vis_w` to the line width.
 */
static void ww_push_span(
    WordWrapAccum *state, char *text, size_t visible_width, ScTextStyle opts
) {
    if (state->span_count == state->span_capacity) {
        state->span_capacity = state->span_capacity ?
            state->span_capacity * 2 : 8;
        state->spans = realloc(
            state->spans, state->span_capacity * sizeof(TSpan)
        );
    }
    state->spans[state->span_count++] = (TSpan){ .text = text, .opts = opts };
    state->span_width += visible_width;
}

/**
 * Emits a word token: flushes the current line first if the word doesn't fit,
 * then hard-breaks it when it exceeds `wrap_w`, otherwise appends it.
 */
static void emit_word_tok(WordWrapAccum *state, WordWrapToken *tok, int wrap_w) {
    if (
        state->span_count > 0 &&
        (int)(state->span_width + tok->visible_width) > wrap_w
    ) {
        ww_flush_sbuf(state);
    }
    if ((int)tok->visible_width > wrap_w) {
        hard_break_word(state, tok, wrap_w);
    } else {
        ww_push_span(state, tok->string, tok->visible_width, tok->opts);
        tok->string = NULL;
    }
}

/**
 * Splits a word token wider than `wrap_w` into fit-sized chunks, flushing
 * a new output line after each full chunk. Frees `tok->s` when done.
 */
static void hard_break_word(
    WordWrapAccum *state, WordWrapToken *tok, int wrap_word
) {
    const char *text_ptr = tok->string;
    size_t remaining_bytes = strlen(tok->string);
    while (remaining_bytes > 0) {
        int avail_cols = wrap_word - (int)state->span_width;
        if (avail_cols <= 0) {
            ww_flush_sbuf(state); avail_cols = wrap_word;
        }
        size_t chunk_bytes = sc_utf8_trim_to_cols(text_ptr, avail_cols);
        if (chunk_bytes == 0) {
            break;
        }
        size_t chunk_vis_width = sc_utf8_string_length(text_ptr, chunk_bytes);
        ww_push_span(
            state, strndup(text_ptr, chunk_bytes), chunk_vis_width, tok->opts
        );
        text_ptr += chunk_bytes; remaining_bytes -= chunk_bytes;
        if (remaining_bytes > 0) {
            ww_flush_sbuf(state);
        }
    }
    free(tok->string); tok->string = NULL;
}
