#include "core/render_wrap.h"

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
 * One contiguous run of spaces or non-spaces extracted from a `ScRenderLine`
 * for the word-wrap engine. The string is heap-owned by the token.
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
static void lb_append_line_copy(LineBuilder *builder, const ScRenderLine *line);
static void lb_append_lines_move(
    LineBuilder *builder, ScRenderLine *source, size_t count
);

static ScRenderLine *wrap_one_line(
    const ScRenderLine *line, int max_width, size_t *out_count
);
    static void tokenize_line(
        const ScRenderLine *line, WordWrapToken **out_tokens, size_t *out_count
    );
    static void flush_wrapped_line(LineBuilder *builder);
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


/* ── LineBuilder helpers ────────────────────────────────────────────────── */

/** Initializes an empty `LineBuilder` with starting capacities. */
void lb_init(LineBuilder *builder) {
    // If an initial malloc fails, leave the matching capacity at 0 so the
    // first grow re-allocates instead of writing through a NULL buffer.
    builder->lines = malloc(INITIAL_LINE_CAPACITY * sizeof(ScRenderLine));
    builder->line_count = 0;
    builder->line_capacity = builder->lines ? INITIAL_LINE_CAPACITY : 0;
    builder->spans = malloc(INITIAL_SPAN_CAPACITY * sizeof(ScRenderSpan));
    builder->span_count = 0;
    builder->span_capacity = builder->spans ? INITIAL_SPAN_CAPACITY : 0;
    builder->span_width = 0;
}

/**
 * Appends a heap copy of `text[0..length)` with `style` to the
 * current-line span buffer and adds `visible_width` to the line width.
 */
void lb_push_span(
    LineBuilder *builder, const char *text, size_t length,
    size_t visible_width, ScTextStyle style
) {
    if (builder->span_count == builder->span_capacity) {
        void *grown = sc_dynarray_grow(
            builder->spans, &builder->span_capacity,
            sizeof(ScRenderSpan), INITIAL_SPAN_CAPACITY
        );
        if (!grown) { return; }
        builder->spans = grown;
    }
    char *copy = strndup(text, length);
    if (!copy) { return; }   // skip the span rather than store NULL text
    builder->spans[builder->span_count++] = (ScRenderSpan){
        .text = copy,
        .style = style,
    };
    builder->span_width += visible_width;
}

/**
 * Finalizes the current line: copies the span buffer into a new `ScRenderLine`,
 * appends it to `builder->lines`, and resets the span buffer.
 */
void lb_finalize_line(LineBuilder *builder) {
    if (builder->line_count == builder->line_capacity) {
        void *grown = sc_dynarray_grow(
            builder->lines, &builder->line_capacity,
            sizeof(ScRenderLine), INITIAL_LINE_CAPACITY
        );
        if (!grown) { goto drop; }
        builder->lines = grown;
    }

    ScRenderSpan *spans_copy =
        malloc((builder->span_count + 1) * sizeof(ScRenderSpan));
    if (!spans_copy) { goto drop; }
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
    return;

drop:
    // OOM: drop the pending line but free its span strings so they don't leak
    // (the span buffer owns them until they move into spans_copy).
    for (size_t i = 0; i < builder->span_count; i++) {
        free(builder->spans[i].text);
    }
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
        void *grown = sc_dynarray_grow(
            builder->lines, &builder->line_capacity,
            sizeof(ScRenderLine), INITIAL_LINE_CAPACITY
        );
        if (!grown) { return; }   // OOM: skip copying this line
        builder->lines = grown;
    }
    ScRenderSpan *spans_copy = malloc((line->count + 1) * sizeof(ScRenderSpan));
    if (!spans_copy) { return; }
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
        size_t new_cap = builder->line_capacity;
        while (new_cap < builder->line_count + count) {
            new_cap = new_cap ? new_cap * 2 : INITIAL_LINE_CAPACITY;
        }
        ScRenderLine *grown =
            realloc(builder->lines, new_cap * sizeof(ScRenderLine));
        if (!grown) { return; }   // OOM: drop these lines (no crash)
        builder->lines = grown;
        builder->line_capacity = new_cap;
    }
    memcpy(
        builder->lines + builder->line_count,
        source, count * sizeof(ScRenderLine)
    );
    builder->line_count += count;
}

/** Frees the span buffer; `lines` ownership stays with the caller. */
void lb_release_span_buffer(LineBuilder *builder) {
    free(builder->spans);
    builder->spans = NULL;
}


/* ── sc_wrap_render_lines / sc_free_render_lines ────────────────────────── */

ScRenderLine *sc_wrap_render_lines(
    const ScRenderLine *lines, size_t count, int max_width, size_t *out_count
) {
    LineBuilder builder;
    lb_init(&builder);

    for (size_t i = 0; i < count; i++) {
        if (max_width <= 0 || (int)lines[i].visible_width <= max_width) {
            lb_append_line_copy(&builder, &lines[i]);
            continue;
        }
        size_t wrapped_count;
        ScRenderLine *wrapped = wrap_one_line(
            &lines[i], max_width, &wrapped_count
        );
        lb_append_lines_move(&builder, wrapped, wrapped_count);
        free(wrapped);
    }

    lb_release_span_buffer(&builder);
    *out_count = builder.line_count;
    return builder.lines;
}

/** Frees every span string, then the spans array and the lines array. */
void sc_free_render_lines(ScRenderLine *lines, size_t count) {
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < lines[i].count; j++) {
            free(lines[i].spans[j].text);
        }
        free(lines[i].spans);
    }
    free(lines);
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
    free(builder.spans);
    builder.span_capacity = WRAP_SPAN_CAPACITY;
    builder.spans = malloc(WRAP_SPAN_CAPACITY * sizeof(ScRenderSpan));
    // On OOM reset to an empty buffer; the first push will grow it from zero
    // rather than write through a NULL pointer.
    if (!builder.spans) { builder.span_capacity = 0; }

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
                void *grown = sc_dynarray_grow(
                    tokens, &capacity,
                    sizeof(WordWrapToken), INITIAL_TOKEN_CAPACITY
                );
                if (!grown) { goto done; }   // OOM: stop tokenizing
                tokens = grown;
            }
            char *run = strndup(run_start, run_length);
            if (!run) { goto done; }   // never store a NULL token string
            tokens[count++] = (WordWrapToken){
                .string = run,
                .visible_width = visible_width,
                .style = style,
                .is_space = is_space_run,
            };
        }
    }
done:
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
        free(builder->spans[--builder->span_count].text);
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
        void *grown = sc_dynarray_grow(
            builder->spans, &builder->span_capacity,
            sizeof(ScRenderSpan), INITIAL_SPAN_CAPACITY
        );
        if (!grown) { free(token->string); token->string = NULL; return; }
        builder->spans = grown;
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
        void *grown = sc_dynarray_grow(
            builder->spans, &builder->span_capacity,
            sizeof(ScRenderSpan), INITIAL_SPAN_CAPACITY
        );
        if (!grown) { free(token->string); token->string = NULL; return; }
        builder->spans = grown;
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
