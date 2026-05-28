#include "sparcli.h"
#include "text_internal.h"
#include "internal.h"
#include "table/table_internal.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


/** Initial capacity for the line buffer in a `LineBuilder`. */
#define INITIAL_LINE_CAPACITY 4

/** Initial capacity for the span buffer in a `LineBuilder`. */
#define INITIAL_SPAN_CAPACITY 4

/** Initial capacity for the token array in `tokenize_line`. */
#define INITIAL_TOKEN_CAPACITY 16

/** Initial capacity for `LineBuilder.spans` once it transitions to wrapping. */
#define WRAP_SPAN_CAPACITY 8


/**
 * Accumulator that builds an array of `ScRenderLine` values one span at a time.
 *
 * Used by both `make_cell_lines` (splitting on `\n`) and `wrap_one_line`
 * (word-wrapping). The two former helper structs `LineScanCtx` /
 * `WordWrapAccum` collapse into this single type.
 */
typedef struct LineBuilder {
    /** Completed `ScRenderLine` array; ownership transfers to the caller. */
    ScRenderLine *lines;

    /** Number of completed lines. */
    size_t line_count;

    /** Allocated capacity of `lines`. */
    size_t line_capacity;

    /** Span buffer of the line currently being built. */
    ScRenderSpan *spans;

    /** Number of spans in the current-line buffer. */
    size_t span_count;

    /** Allocated capacity of `spans`. */
    size_t span_capacity;

    /** Visible column width accumulated for the current line. */
    size_t span_width;
} LineBuilder;

/**
 * One contiguous run of spaces or non-spaces extracted from a `ScRenderLine` for
 * the word-wrap engine. The string is heap-owned by the token.
 */
typedef struct WordWrapToken {
    /** Heap-allocated UTF-8 run; transferred or freed during emission. */
    char *string;

    /** Visible width of `string` in columns. */
    size_t visible_width;

    /** Style of the span this token came from. */
    ScTextStyle style;

    /** `true` for runs of spaces, `false` for words. */
    bool is_space;
} WordWrapToken;


// Forward declarations indented to reflect call hierarchy
static void lb_init(LineBuilder *builder);
static void lb_push_span(
    LineBuilder *builder, const char *text, size_t length,
    size_t visible_width, ScTextStyle style
);
static void lb_finalize_line(LineBuilder *builder);
static void lb_append_line_copy(LineBuilder *builder, const ScRenderLine *line);
static void lb_append_lines_move(
    LineBuilder *builder, ScRenderLine *source, size_t count
);
static void lb_release_span_buffer(LineBuilder *builder);

static void scan_text_into_builder(
    const char *text, ScTextStyle style, LineBuilder *builder
);
static void resolve_cell_span(
    const ScCell *cell, size_t span_index,
    const char **out_text, ScTextStyle *out_style
);

static ScRenderLine *wrap_one_line(
    const ScRenderLine *line, int max_width, size_t *out_count
);
    static void tokenize_line(
        const ScRenderLine *line, WordWrapToken **out_tokens, size_t *out_count
    );
    static void emit_space_token(
        LineBuilder *builder, WordWrapToken *token,
        const WordWrapToken *next_token, int max_width
    );
    static void emit_word_token(
        LineBuilder *builder, WordWrapToken *token, int max_width
    );
        static void hard_break_word(
            LineBuilder *builder, WordWrapToken *token, int max_width
        );


/* ── make_cell_lines ────────────────────────────────────────────────────── */

/**
 * Splits `cell` content on `\n` into an array of `ScRenderLine` values.
 *
 * Each `ScRenderLine` owns heap copies of its span strings; free with
 * `free_tlines`.
 *
 * @param cell       Source cell.
 * @param out_count  Receives the number of returned lines.
 * @return           Heap-allocated `ScRenderLine` array (length `*out_count`).
 */
ScRenderLine *make_cell_lines(const ScCell *cell, size_t *out_count) {
    LineBuilder builder;
    lb_init(&builder);

    bool is_rich = (cell->kind == SC_CELL_TEXT
                    || cell->kind == SC_CELL_MARKUP) && cell->text;
    size_t span_count = is_rich ? cell->text->count : 1;

    for (size_t span_index = 0; span_index < span_count; span_index++) {
        const char *text;
        ScTextStyle style;
        resolve_cell_span(cell, span_index, &text, &style);
        scan_text_into_builder(text, style, &builder);
    }
    lb_finalize_line(&builder);
    lb_release_span_buffer(&builder);

    *out_count = builder.line_count;
    return builder.lines;
}

/**
 * Returns the visible width of the widest line in `cell`.
 */
size_t cell_visible_width(const ScCell *cell) {
    size_t line_count;
    ScRenderLine *lines = make_cell_lines(cell, &line_count);
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
 * Builds the cell's lines just to count them and immediately frees them.
 * Honors word-wrap when enabled and `available_width > 0`.
 */
size_t count_cell_lines(
    const ScCell *cell, const ScColOpts *col_opts, int available_width
) {
    size_t line_count;
    ScRenderLine *lines = (col_opts->word_wrap && available_width > 0)
        ? wrap_cell_lines(cell, available_width, &line_count)
        : make_cell_lines(cell, &line_count);
    free_tlines(lines, line_count);
    return line_count;
}

/** Frees every span string, then the spans array and the lines array. */
void free_tlines(ScRenderLine *lines, size_t line_count) {
    for (size_t i = 0; i < line_count; i++) {
        for (size_t j = 0; j < lines[i].count; j++) {
            free((char *)lines[i].spans[j].text);
        }
        free(lines[i].spans);
    }
    free(lines);
}


/* ── wrap_cell_lines ────────────────────────────────────────────────────── */

/**
 * Returns word-wrapped copies of every line in `cell` longer than
 * `max_width`; lines that already fit are deep-copied unchanged.
 */
ScRenderLine *wrap_cell_lines(
    const ScCell *cell, int max_width, size_t *out_count
) {
    size_t raw_count;
    ScRenderLine *raw_lines = make_cell_lines(cell, &raw_count);
    if (max_width <= 0) {
        *out_count = raw_count;
        return raw_lines;
    }

    LineBuilder builder;
    lb_init(&builder);

    for (size_t i = 0; i < raw_count; i++) {
        if ((int)raw_lines[i].visible_width <= max_width) {
            lb_append_line_copy(&builder, &raw_lines[i]);
            continue;
        }
        size_t wrapped_count;
        ScRenderLine *wrapped = wrap_one_line(
            &raw_lines[i], max_width, &wrapped_count
        );
        lb_append_lines_move(&builder, wrapped, wrapped_count);
        free(wrapped);
    }

    free_tlines(raw_lines, raw_count);
    lb_release_span_buffer(&builder);
    *out_count = builder.line_count;
    return builder.lines;
}


/* ── LineBuilder helpers ────────────────────────────────────────────────── */

/** Initializes an empty `LineBuilder` with starting capacities. */
static void lb_init(LineBuilder *builder) {
    builder->lines = malloc(INITIAL_LINE_CAPACITY * sizeof(ScRenderLine));
    builder->line_count = 0;
    builder->line_capacity = INITIAL_LINE_CAPACITY;
    builder->spans = malloc(INITIAL_SPAN_CAPACITY * sizeof(ScRenderSpan));
    builder->span_count = 0;
    builder->span_capacity = INITIAL_SPAN_CAPACITY;
    builder->span_width = 0;
}

/**
 * Appends a heap copy of `text[0..length)` with `style` to the
 * current-line span buffer and adds `visible_width` to the line width.
 */
static void lb_push_span(
    LineBuilder *builder, const char *text, size_t length,
    size_t visible_width, ScTextStyle style
) {
    if (builder->span_count == builder->span_capacity) {
        builder->span_capacity *= 2;
        builder->spans = realloc(
            builder->spans, builder->span_capacity * sizeof(ScRenderSpan)
        );
    }
    builder->spans[builder->span_count++] = (ScRenderSpan){
        .text = strndup(text, length),
        .style = style,
    };
    builder->span_width += visible_width;
}

/**
 * Finalizes the current line: copies the span buffer into a new `ScRenderLine`,
 * appends it to `builder->lines`, and resets the span buffer.
 */
static void lb_finalize_line(LineBuilder *builder) {
    if (builder->line_count == builder->line_capacity) {
        builder->line_capacity *= 2;
        builder->lines = realloc(
            builder->lines, builder->line_capacity * sizeof(ScRenderLine)
        );
    }

    ScRenderSpan *spans_copy = malloc((builder->span_count + 1) * sizeof(ScRenderSpan));
    if (builder->span_count > 0) {
        memcpy(
            spans_copy, builder->spans,
            builder->span_count * sizeof(ScRenderSpan)
        );
    }
    builder->lines[builder->line_count++] = (ScRenderLine){
        .spans = spans_copy,
        .count = builder->span_count,
        .visible_width = builder->span_width,
    };
    builder->span_count = 0;
    builder->span_width = 0;
}

/**
 * Deep-copies `line` (spans + their strings) into a fresh `ScRenderLine` and
 * appends it to `builder->lines`. Does not touch the span buffer.
 */
static void lb_append_line_copy(
    LineBuilder *builder, const ScRenderLine *line
) {
    if (builder->line_count == builder->line_capacity) {
        builder->line_capacity *= 2;
        builder->lines = realloc(
            builder->lines, builder->line_capacity * sizeof(ScRenderLine)
        );
    }
    ScRenderSpan *spans_copy = malloc((line->count + 1) * sizeof(ScRenderSpan));
    for (size_t i = 0; i < line->count; i++) {
        spans_copy[i] = (ScRenderSpan){
            .text = strdup(line->spans[i].text),
            .style = line->spans[i].style,
        };
    }
    builder->lines[builder->line_count++] = (ScRenderLine){
        .spans = spans_copy,
        .count = line->count,
        .visible_width = line->visible_width,
    };
}

/**
 * Moves `count` already-built lines from `source` into `builder->lines`
 * (transfers span ownership; `source` is left as a shallow array).
 */
static void lb_append_lines_move(
    LineBuilder *builder, ScRenderLine *source, size_t count
) {
    if (builder->line_count + count > builder->line_capacity) {
        while (builder->line_capacity < builder->line_count + count) {
            builder->line_capacity = builder->line_capacity
                ? builder->line_capacity * 2 : INITIAL_LINE_CAPACITY;
        }
        builder->lines = realloc(
            builder->lines, builder->line_capacity * sizeof(ScRenderLine)
        );
    }
    memcpy(
        builder->lines + builder->line_count,
        source, count * sizeof(ScRenderLine)
    );
    builder->line_count += count;
}

/** Frees the span buffer; `lines` ownership stays with the caller. */
static void lb_release_span_buffer(LineBuilder *builder) {
    free(builder->spans);
    builder->spans = NULL;
}


/* ── Newline scanner ────────────────────────────────────────────────────── */

/**
 * Scans `text` for `\n` boundaries, pushing each segment into the builder
 * via `lb_push_span` and finalizing a line on every newline. Any tail
 * after the last newline stays in the buffer for the caller to flush.
 */
static void scan_text_into_builder(
    const char *text, ScTextStyle style, LineBuilder *builder
) {
    const char *segment_start = text;
    const char *cursor = text;
    while (*cursor) {
        if (*cursor == '\n') {
            size_t length = (size_t)(cursor - segment_start);
            if (length > 0) {
                size_t visible_width = sc_utf8_string_length(
                    segment_start, length
                );
                lb_push_span(
                    builder, segment_start, length, visible_width, style
                );
            }
            lb_finalize_line(builder);
            segment_start = cursor + 1;
        }
        cursor++;
    }
    size_t tail_length = (size_t)(cursor - segment_start);
    if (tail_length > 0) {
        size_t visible_width = sc_utf8_string_length(
            segment_start, tail_length
        );
        lb_push_span(
            builder, segment_start, tail_length, visible_width, style
        );
    }
}

/**
 * Resolves the raw text and style for span `span_index` of `cell`.
 * `SC_CELL_STR` cells always use a single unstyled span.
 */
static void resolve_cell_span(
    const ScCell *cell, size_t span_index,
    const char **out_text, ScTextStyle *out_style
) {
    static const ScTextStyle plain = {
        SC_TEXT_ATTR_NONE,
        { -2, 0, 0, 0 },
        { -2, 0, 0, 0 },
    };

    if (cell->kind == SC_CELL_STR) {
        *out_text = cell->str ? cell->str : "";
        *out_style = plain;
        return;
    }
    *out_text = cell->text->spans[span_index].raw_str;
    *out_style = cell->text->spans[span_index].style;
}


/* ── Word-wrap engine ───────────────────────────────────────────────────── */

/**
 * Wraps `line` into multiple lines each fitting within `max_width` columns.
 * The returned array is a shallow `ScRenderLine[]` produced by an internal
 * `LineBuilder`; the caller takes ownership.
 */
static ScRenderLine *wrap_one_line(
    const ScRenderLine *line, int max_width, size_t *out_count
) {
    WordWrapToken *tokens;
    size_t token_count;
    tokenize_line(line, &tokens, &token_count);

    LineBuilder builder;
    lb_init(&builder);
    builder.span_capacity = WRAP_SPAN_CAPACITY;
    free(builder.spans);
    builder.spans = malloc(WRAP_SPAN_CAPACITY * sizeof(ScRenderSpan));

    for (size_t i = 0; i < token_count; i++) {
        const WordWrapToken *next =
            (i + 1 < token_count) ? &tokens[i + 1] : NULL;
        if (tokens[i].is_space) {
            emit_space_token(&builder, &tokens[i], next, max_width);
        } else {
            emit_word_token(&builder, &tokens[i], max_width);
        }
    }
    lb_finalize_line(&builder);
    lb_release_span_buffer(&builder);

    for (size_t i = 0; i < token_count; i++) {
        if (tokens[i].string) { free(tokens[i].string); }
    }
    free(tokens);

    *out_count = builder.line_count;
    return builder.lines;
}

/**
 * Splits every span of `line` into alternating space-runs and word-runs.
 * The caller owns the returned array and must free each token's `string`.
 */
static void tokenize_line(
    const ScRenderLine *line, WordWrapToken **out_tokens, size_t *out_count
) {
    WordWrapToken *tokens = NULL;
    size_t count = 0;
    size_t capacity = 0;

    for (size_t span_index = 0; span_index < line->count; span_index++) {
        const char *cursor = line->spans[span_index].text;
        ScTextStyle style = line->spans[span_index].style;
        while (*cursor) {
            const char *run_start = cursor;
            bool is_space_run = (*cursor == ' ');
            while (*cursor && (*cursor == ' ') == is_space_run) {
                cursor++;
            }
            size_t run_length = (size_t)(cursor - run_start);
            size_t visible_width = is_space_run
                ? run_length
                : sc_utf8_string_length(run_start, run_length);

            if (count == capacity) {
                capacity = capacity
                    ? capacity * 2 : INITIAL_TOKEN_CAPACITY;
                tokens = realloc(tokens, capacity * sizeof(WordWrapToken));
            }
            tokens[count++] = (WordWrapToken){
                .string = strndup(run_start, run_length),
                .visible_width = visible_width,
                .style = style,
                .is_space = is_space_run,
            };
        }
    }
    *out_tokens = tokens;
    *out_count = count;
}

/**
 * Trims trailing space spans from the current line, finalizes the line,
 * and resets the span buffer.
 */
static void flush_wrapped_line(LineBuilder *builder) {
    while (builder->span_count > 0
        && builder->spans[builder->span_count - 1].text[0] == ' ') {
        builder->span_width -=
            strlen(builder->spans[builder->span_count - 1].text);
        free((char *)builder->spans[--builder->span_count].text);
    }
    lb_finalize_line(builder);
}

/**
 * Emits a space token: drops it at line start, flushes-then-drops it when
 * the next word would no longer fit, otherwise appends it.
 */
static void emit_space_token(
    LineBuilder *builder, WordWrapToken *token,
    const WordWrapToken *next_token, int max_width
) {
    if (builder->span_count == 0) {
        free(token->string);
        token->string = NULL;
        return;
    }
    if (next_token && !next_token->is_space
        && (int)(builder->span_width + token->visible_width
                 + next_token->visible_width) > max_width) {
        free(token->string);
        token->string = NULL;
        flush_wrapped_line(builder);
        return;
    }

    if (builder->span_count == builder->span_capacity) {
        builder->span_capacity *= 2;
        builder->spans = realloc(
            builder->spans, builder->span_capacity * sizeof(ScRenderSpan)
        );
    }
    builder->spans[builder->span_count++] = (ScRenderSpan){
        .text = token->string,
        .style = token->style,
    };
    builder->span_width += token->visible_width;
    token->string = NULL;
}

/**
 * Emits a word token: flushes the current line first when the word does
 * not fit, then hard-breaks it when it exceeds `max_width` on its own.
 */
static void emit_word_token(
    LineBuilder *builder, WordWrapToken *token, int max_width
) {
    if (builder->span_count > 0
        && (int)(builder->span_width + token->visible_width) > max_width) {
        flush_wrapped_line(builder);
    }
    if ((int)token->visible_width > max_width) {
        hard_break_word(builder, token, max_width);
        return;
    }

    if (builder->span_count == builder->span_capacity) {
        builder->span_capacity *= 2;
        builder->spans = realloc(
            builder->spans, builder->span_capacity * sizeof(ScRenderSpan)
        );
    }
    builder->spans[builder->span_count++] = (ScRenderSpan){
        .text = token->string,
        .style = token->style,
    };
    builder->span_width += token->visible_width;
    token->string = NULL;
}

/**
 * Splits a word wider than `max_width` into fit-sized chunks, flushing a
 * new output line after each full chunk. Frees `token->string` when done.
 */
static void hard_break_word(
    LineBuilder *builder, WordWrapToken *token, int max_width
) {
    const char *cursor = token->string;
    size_t remaining_bytes = strlen(token->string);

    while (remaining_bytes > 0) {
        int available = max_width - (int)builder->span_width;
        if (available <= 0) {
            flush_wrapped_line(builder);
            available = max_width;
        }
        size_t chunk_bytes = sc_utf8_trim_to_cols(cursor, available);
        if (chunk_bytes == 0) { break; }

        size_t chunk_width = sc_utf8_string_length(cursor, chunk_bytes);
        lb_push_span(
            builder, cursor, chunk_bytes, chunk_width, token->style
        );
        cursor += chunk_bytes;
        remaining_bytes -= chunk_bytes;
        if (remaining_bytes > 0) { flush_wrapped_line(builder); }
    }
    free(token->string);
    token->string = NULL;
}
