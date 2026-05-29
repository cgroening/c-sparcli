#pragma once

#include "sparcli.h"
#include "internal.h"

#include <stddef.h>


/**
 * Accumulator that builds an array of `ScRenderLine` values one span at a time.
 *
 * Shared by the table cell builder (splitting on `\n`) and the word-wrap engine
 * (`sc_wrap_render_lines`). Both the panel and table renderers rely on it.
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


/* ── LineBuilder helpers ────────────────────────────────────────────────── */

/** Initializes an empty `LineBuilder` with starting capacities. */
void lb_init(LineBuilder *builder);

/**
 * Appends a heap copy of `text[0..length)` with `style` to the
 * current-line span buffer and adds `visible_width` to the line width.
 */
void lb_push_span(
    LineBuilder *builder, const char *text, size_t length,
    size_t visible_width, ScTextStyle style
);

/**
 * Finalizes the current line: copies the span buffer into a new `ScRenderLine`,
 * appends it to `builder->lines`, and resets the span buffer.
 */
void lb_finalize_line(LineBuilder *builder);

/** Frees the span buffer; `lines` ownership stays with the caller. */
void lb_release_span_buffer(LineBuilder *builder);


/* ── Word-wrap engine ───────────────────────────────────────────────────── */

/**
 * Returns word-wrapped copies of every line in `lines` longer than
 * `max_width`; lines that already fit are deep-copied unchanged. Words wider
 * than `max_width` are hard-broken. The returned array is heap-allocated and
 * owned by the caller; free it with `sc_free_render_lines`.
 *
 * When `max_width <= 0`, every line is deep-copied unchanged.
 */
ScRenderLine *sc_wrap_render_lines(
    const ScRenderLine *lines, size_t count, int max_width, size_t *out_count
);

/** Frees every span string, then the spans array and the lines array. */
void sc_free_render_lines(ScRenderLine *lines, size_t count);
