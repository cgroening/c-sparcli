#pragma once
#include "core/sparcli_export.h"


#include <stddef.h>

SPARCLI_BEGIN_DECLS

/**
 * A captured, line-split rendering of any widget.
 *
 * Produced by the `sc_capture_*` family of functions. Each line retains its
 * original ANSI escape codes; `column_widths` holds the corresponding visible
 * column count so callers never need to re-parse escape sequences for
 * layout arithmetic.
 *
 * Pass to `sc_pad_print()`, `sc_align_print()` or `sc_columns_add_rendered()`.
 * Free with `sc_rendered_free()`.
 */
typedef struct ScRendered {
    /** Heap-allocated strings, ANSI codes included. */
    char **lines;

    /** Visible column width per line, parallel to lines. */
    int *column_widths;

    /** Number of lines. */
    size_t line_count;

    /** Maximum visible width across all lines. */
    int max_column_width;
} ScRendered;

SPARCLI_EXPORT void sc_rendered_free(ScRendered *r);

SPARCLI_END_DECLS
