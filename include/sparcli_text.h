#pragma once

#include <stddef.h>
#include "sparcli_core.h"


/**
 * ScSpan - A single styled run of text within an ScText.
 *
 * `raw_str` holds the raw UTF-8 bytes (no ANSI codes); the style is applied
 * at render time. A span may contain newlines to produce multi-line output.
 */
typedef struct {
    char       *raw_str; /**< Heap-allocated UTF-8 string, no ANSI codes. */
    ScTextStyle style;  /**< Color and text attributes applied at render time. */
} ScSpan;

/**
 * ScText - A sequence of styled text spans.
 *
 * Build with `sc_text_new()` and `sc_text_append()`, free with `sc_text_free()`.
 * The spans array is grown automatically as spans are appended.
 */
typedef struct {
    ScSpan *spans;  /**< Heap-allocated array of spans. */
    size_t  count;  /**< Number of spans currently in use. */
    size_t  capacity;  /**< Allocated capacity of the spans array. */
} ScText;


/**
 * Allocates an empty `ScText` with no spans.
 */
ScText *sc_text_new(void);

/**
 * Allocates an `ScText` containing `s` as a single unstyled span.
 *
 * Convenience wrapper around `sc_text_new` + `sc_text_append` for the common
 * case of wrapping a plain string. Caller must free the result with
 * `sc_text_free`.
 */
ScText *sc_text_from_str(const char *s);

/**
 * Appends a new span to `sc_text` with the given raw UTF-8 string and style.
 *
 * The spans array is doubled in capacity when full, starting at 4. `raw_string`
 * is duplicated internally; the caller retains ownership of the original.
 */
void sc_text_append(ScText *sc_text, const char *raw_str, ScTextStyle style);

/**
 * Frees all spans and the `ScText` itself.
 */
void sc_text_free(ScText *sc_text);

/**
 * Prints all spans of `sc_text` to stdout, applying each span's style.
 *
 * Newlines within a span's raw_str are passed through as-is; the style is
 * applied per segment between newlines so line breaks do not carry ANSI codes.
 */
void sc_print_text(const ScText *sc_text);


/**
 * Returns the maximum visible column width across all lines of text.
 *
 * Counts UTF-8 codepoints (skipping continuation bytes) and resets the
 * counter at each newline, returning the widest line seen.
 */
size_t sc_text_visible_width(const ScText *sc_text);
