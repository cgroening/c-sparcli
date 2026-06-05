#pragma once

#include "output/sparcli_progressbar.h"

#include <stdbool.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_multiprogress.h
 * @brief Several progress bars updated together, in place.
 *
 * Wraps a set of @ref ScProgressBar instances and an internal live display:
 * each bar occupies its own line and `sc_multiprogress_update` redraws the
 * whole stack in place, so concurrent tasks each get their own bar (like
 * a multi-bar download view). Built on the existing progress-bar renderer
 * and the live-display engine, so it inherits their behavior: off a
 * terminal (pipe/file/CI) updates are buffered and only the final stack is
 * printed.
 *
 * @code
 * ScMultiProgress *mp = sc_multiprogress_begin((ScMultiProgressOpts){ 0 });
 * int a = sc_multiprogress_add(mp, "download",
 *                              (ScProgressBarOpts){ .show_percent = true });
 * int b = sc_multiprogress_add(mp, "extract",
 *                              (ScProgressBarOpts){ .show_percent = true });
 * for (int i = 0; i <= 100; i++) {
 *     sc_multiprogress_update(mp, a, i, 100);
 *     sc_multiprogress_update(mp, b, i / 2, 100);
 *     usleep(30000);
 * }
 * sc_multiprogress_end(mp);
 * @endcode
 */


/** Opaque multi-progress session. */
typedef struct ScMultiProgress ScMultiProgress;

/** Options for @ref sc_multiprogress_begin. Zero-init = sensible defaults. */
typedef struct ScMultiProgressOpts {
    /** Erase the bars when the session ends instead of leaving them. */
    bool transient;

    /** Emit redraw escapes even off a terminal (default: buffer, print the
        final stack once). */
    bool always;
} ScMultiProgressOpts;


/**
 * Starts a multi-progress session on the current output stream.
 * @return  Heap handle (pair with @ref sc_multiprogress_end); `NULL` on OOM.
 */
SPARCLI_EXPORT ScMultiProgress *sc_multiprogress_begin(
    ScMultiProgressOpts opts
);

/**
 * Adds a bar with `label` and the given bar options.
 *
 * @param mp     Session handle.
 * @param label  Bar label (copied); may be `NULL`.
 * @param opts   Per-bar rendering options (copied, like
 *               @ref sc_progressbar_new).
 * @return       The new bar's index, or `-1` on failure.
 */
SPARCLI_EXPORT int sc_multiprogress_add(
    ScMultiProgress *mp, const char *label, ScProgressBarOpts opts
);

/**
 * Updates bar `index` to `value`/`max` and redraws the whole stack.
 * (`max == 0` interprets `value` as a 0..1 ratio, like the single bar.)
 */
SPARCLI_EXPORT void sc_multiprogress_update(
    ScMultiProgress *mp, int index, double value, double max
);

/** Replaces the label of bar `index` and redraws. */
SPARCLI_EXPORT void sc_multiprogress_set_label(
    ScMultiProgress *mp, int index, const char *label
);

/** Ends the session (leaves the final stack unless `transient`) and frees
 *  the handle and all bars. `NULL`-safe. */
SPARCLI_EXPORT void sc_multiprogress_end(ScMultiProgress *mp);

SPARCLI_END_DECLS
