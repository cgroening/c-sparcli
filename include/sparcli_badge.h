#pragma once

#include "sparcli_core.h"
#include "sparcli_text.h"

SPARCLI_BEGIN_DECLS


/**
 * Rendering options for an inline badge token.
 *
 * Badge string: `left_cap + pad×' ' + text + pad×' ' + right_cap`.
 */
typedef struct ScBadgeOpts {
    /** Left bracket string; `NULL` = `"["`. */
    const char *left_cap;

    /** Right bracket string; `NULL` = `"]"`. */
    const char *right_cap;

    /**
     * Style applied to the full badge (caps, padding and text);
     * zero-init = no formatting.
     */
    ScTextStyle text_style;

    /** Spaces inserted inside each cap; default `0`. */
    int pad;
} ScBadgeOpts;


/**
 * Prints a styled badge token to stdout (no trailing newline).
 *
 * @param text  Badge text; may be `NULL` to print just the caps and padding.
 * @param opts  Rendering options.
 */
SPARCLI_EXPORT void sc_print_badge(const char *text, ScBadgeOpts opts);

/**
 * Appends the composed badge string as a single span to `text_obj`.
 *
 * The full badge (caps + padding + text) is added as one span with
 * `opts.text_style` applied; the original `text_obj` retains ownership of
 * its existing spans.
 *
 * @param text_obj  Rich-text object the badge is appended to.
 * @param text      Badge text; may be `NULL`.
 * @param opts      Rendering options.
 */
SPARCLI_EXPORT void sc_text_append_badge(
    ScText *text_obj, const char *text, ScBadgeOpts opts
);

SPARCLI_END_DECLS
