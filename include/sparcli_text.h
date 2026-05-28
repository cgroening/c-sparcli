#pragma once

#include "sparcli_core.h"
#include "sparcli_export.h"

#include <stddef.h>


SPARCLI_BEGIN_DECLS

/**
 * ScSpan - A single styled run of text within an ScText.
 *
 * `raw_str` holds the raw UTF-8 bytes (no ANSI codes); the style is applied
 * at render time. A span may contain newlines to produce multi-line output.
 */
typedef struct {
    char       *raw_str;  /**< Heap-allocated UTF-8 string, no ANSI codes. */
    ScTextStyle style;    /**< Style applied at render time. */
} ScSpan;

/**
 * ScText - A sequence of styled text spans.
 *
 * Build with `sc_text_new()` and `sc_text_append()`, free with `sc_text_free()`.
 * The spans array is grown automatically as spans are appended.
 */
typedef struct {
    ScSpan *spans;     /**< Heap-allocated array of spans. */
    size_t  count;     /**< Number of spans currently in use. */
    size_t  capacity;  /**< Allocated capacity of the spans array. */
} ScText;


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
 * Convenience wrapper around `sc_text_new` + `sc_text_append`.
 *
 * @param str  Source string; copied internally; caller retains ownership.
 *             `NULL` is treated as an empty string.
 * @return     Heap-allocated text; `NULL` on allocation failure.
 */
SPARCLI_EXPORT ScText *sc_text_from_str(const char *str);

/**
 * Appends a new span to `text` with the given UTF-8 string and style.
 *
 * `raw_str` is `strdup`-ed internally; the caller retains ownership of
 * the original.
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
 * Counts UTF-8 codepoints (skipping continuation bytes) and resets the
 * counter at each newline, returning the widest line seen.
 *
 * @param text  Text to measure; returns `0` when `NULL`.
 * @return      Widest line width in terminal columns.
 */
SPARCLI_EXPORT size_t sc_text_visible_width(const ScText *text);

SPARCLI_END_DECLS
