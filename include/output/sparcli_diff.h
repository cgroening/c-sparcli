#pragma once

#include "core/sparcli_core.h"
#include "core/sparcli_text.h"
#include "core/sparcli_rendered.h"
#include "core/sparcli_export.h"


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_diff.h
 * @brief Colored line-based unified diff of two texts.
 *
 * Computes a minimal line diff (LCS) between two texts and renders it in
 * the familiar unified-diff style: `@@ … @@` hunk headers, ` ` context,
 * `-` removed (red) and `+` added (green) lines. The diffed text is
 * sanitized at the boundary like any other user string.
 */


/** Options for the diff renderer. Zero-init = 3 lines of context, the
 *  default red/green/cyan colors and `old`/`new` header labels. */
typedef struct ScDiffOpts {
    /** Lines of unchanged context around each change. `0` = the default
        (3); a negative value shows the full files (no elision). */
    int context;

    /** Suppress the `--- old` / `+++ new` file header lines. */
    bool no_header;

    /** Label for the old/left side in the header; `NULL` = `"old"`. */
    const char *old_label;

    /** Label for the new/right side in the header; `NULL` = `"new"`. */
    const char *new_label;

    /** Color of added (`+`) lines; zero-init = green. */
    ScColor add_color;

    /** Color of removed (`-`) lines; zero-init = red. */
    ScColor del_color;

    /** Color of `@@` hunk headers; zero-init = cyan. */
    ScColor hunk_color;
} ScDiffOpts;


/**
 * Builds the colored unified diff of `old_text` vs `new_text` as an
 * `ScText` (one styled span per line). Returns an empty `ScText` when the
 * inputs are identical.
 *
 * @return  Heap `ScText` (free with `sc_text_free`); `NULL` on OOM.
 */
SPARCLI_EXPORT ScText *sc_diff_text(
    const char *old_text, const char *new_text, ScDiffOpts opts
);

/** Prints the unified diff to the current output stream. */
SPARCLI_EXPORT void sc_diff_print(
    const char *old_text, const char *new_text, ScDiffOpts opts
);

/**
 * Renders the unified diff into a heap `ScRendered` (free with
 * `sc_rendered_free`), usable with `sc_pad_print`, panels, columns, etc.
 */
SPARCLI_EXPORT ScRendered *sc_capture_diff(
    const char *old_text, const char *new_text, ScDiffOpts opts
);

SPARCLI_END_DECLS
