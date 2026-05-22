#pragma once

#include <stddef.h>

/**
 * ScRendered Struct - A captured, line-split rendering of any widget.
 *
 * Produced by the `sc_capture_*` family of functions. Each line retains its
 * original ANSI escape codes; `column_widths` holds the corresponding visible
 * column count so callers never need to re-parse escape sequences for
 * layout arithmetic.
 *
 * Pass to `sc_pad_print()`, `sc_align_print()` or `sc_columns_add_rendered()`.
 * Free with `sc_rendered_free()`.
 */
typedef struct {
    char   **lines;          /**< Heap-allocated strings, ANSI codes included. */
    int     *column_widths;  /**< Visible column width per line, parallel to lines. */
    size_t   line_count;        /**< Number of lines. */
    int      max_column_width;  /**< Maximum visible width across all lines. */
} ScRendered;

void sc_rendered_free(ScRendered *r);
