#pragma once

#include "core/sparcli_core.h"
#include "core/sparcli_export.h"

#include <stddef.h>


SPARCLI_BEGIN_DECLS

/**
 * A single styled run of text within an ScText.
 *
 * `raw_str` holds the raw UTF-8 bytes (no ANSI codes); the style is
 * applied at render time. A span may contain newlines to produce
 * multi-line output.
 */
typedef struct ScSpan {
    /** Heap-allocated UTF-8 string, no ANSI codes. */
    char *raw_str;

    /** Style applied at render time. */
    ScTextStyle style;
} ScSpan;

/**
 * A sequence of styled text spans.
 *
 * Opaque type; construct via `sc_text_new`, free via `sc_text_free`,
 * inspect via the accessors below. Layout is implementation-defined and
 * may change across versions; bindings must go through the API.
 */
typedef struct ScText ScText;


/**
 * Allocates an empty `ScText` with no spans.
 *
 * @return  Heap-allocated empty text; `NULL` on allocation failure.
 *          Caller must free with `sc_text_free`.
 */
SPARCLI_EXPORT ScText *sc_text_new(void);

/**
 * Allocates an `ScText` containing `str` as a single unstyled span.
 *
 * @param str  Source string; copied internally; caller retains ownership.
 *             `NULL` is treated as an empty string.
 * @return     Heap-allocated text; `NULL` on allocation failure.
 */
SPARCLI_EXPORT ScText *sc_text_from_str(const char *str);

/**
 * Appends a new span to `text` with the given UTF-8 string and style.
 *
 * `raw_str` is duplicated internally; the caller retains ownership.
 *
 * @param text     Target text; no-op when `NULL`.
 * @param raw_str  UTF-8 string; no-op when `NULL`.
 * @param style    Style applied to this span.
 */
SPARCLI_EXPORT void sc_text_append(
    ScText *text, const char *raw_str, ScTextStyle style
);

/**
 * Frees `text` and all owned spans.
 *
 * @param text  Object to free; safe to pass `NULL`.
 */
SPARCLI_EXPORT void sc_text_free(ScText *text);

/**
 * Prints all spans of `text` to the current output stream.
 *
 * Newlines within a span's `raw_str` are passed through as-is; the style
 * is applied per segment between newlines so line breaks do not carry
 * ANSI codes.
 *
 * @param text  Text to print; no-op when `NULL`.
 */
SPARCLI_EXPORT void sc_print_text(const ScText *text);

/**
 * Returns the maximum visible column width across all lines of `text`.
 *
 * Counts UTF-8 codepoints and resets the counter at each newline.
 *
 * @param text  Text to measure; returns `0` when `NULL`.
 */
SPARCLI_EXPORT size_t sc_text_visible_width(const ScText *text);


/* ── Accessors (FFI-friendly) ──────────────────────────────────────────── */

/**
 * Returns the number of spans currently in `text`.
 *
 * @param text  Text to inspect; returns `0` when `NULL`.
 */
SPARCLI_EXPORT size_t sc_text_span_count(const ScText *text);

/**
 * Returns the span at `index` (by-value copy).
 *
 * Out-of-range `index` or a `NULL` `text` returns a zero-initialized
 * `ScSpan` (`{ NULL, … }`).
 *
 * @param text   Text to inspect.
 * @param index  Zero-based span index.
 */
SPARCLI_EXPORT ScSpan sc_text_span(const ScText *text, size_t index);

SPARCLI_END_DECLS
