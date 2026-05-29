#pragma once

#include "core/sparcli_rendered.h"
#include "core/sparcli_text.h"

SPARCLI_BEGIN_DECLS


/**
 * Padding values applied around a captured widget.
 *
 * `top` and `bottom` are emitted as blank lines; `left` and `right` are
 * spaces prepended and appended to every content line. Trailing `right`
 * spaces are mostly useful in composed contexts (coloured backgrounds);
 * they have no visible effect on a plain terminal.
 */
typedef struct ScPadOpts {
    int top;    /**< Blank lines printed before the content. */
    int right;  /**< Spaces appended to each content line. */
    int bottom; /**< Blank lines printed after the content. */
    int left;   /**< Spaces prepended to each content line. */
} ScPadOpts;


/**
 * Prints `rendered` with the given padding around each line.
 *
 * @param rendered  Captured widget; no output when `NULL`.
 * @param opts      Padding values; negative values are treated as zero.
 */
SPARCLI_EXPORT void sc_pad_print(const ScRendered *rendered, ScPadOpts opts);

/**
 * Convenience wrapper: captures `str` and prints it with the given padding.
 *
 * @param str   Plain string to render.
 * @param opts  Padding values.
 */
SPARCLI_EXPORT void sc_pad_str(const char *str, ScPadOpts opts);

/**
 * Convenience wrapper: captures `text` and prints it with the given padding.
 *
 * @param text  Rich-text object to render.
 * @param opts  Padding values.
 */
SPARCLI_EXPORT void sc_pad_text(const ScText *text, ScPadOpts opts);

SPARCLI_END_DECLS
