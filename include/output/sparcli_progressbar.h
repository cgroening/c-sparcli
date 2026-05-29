#pragma once

#include "core/sparcli_core.h"

SPARCLI_BEGIN_DECLS


/**
 * Visual style of the progress bar's fill and empty cells.
 */
typedef enum ScProgressType {
    SC_PROGRESS_BLOCK,  /**< `█` fill, `░` empty (Unicode blocks) */
    SC_PROGRESS_ASCII,  /**< `=` fill with `>` edge, space empty */
    SC_PROGRESS_LINE,   /**< `━` fill, `╌` empty (heavy horizontal) */
    SC_PROGRESS_SHADED, /**< `▓` fill with `▒` edge, `░` empty (shaded blocks) */
} ScProgressType;

/**
 * Optional color thresholds for the bar's fill.
 *
 * When `enabled` is `true`, the fill color switches based on the current
 * ratio: `< mid` uses `color_low`, `>= mid` uses `color_mid`,
 * `>= high` uses `color_high`.
 */
typedef struct ScProgressThresholds {
    /** When `true`, the fill color is selected from the ratio. */
    bool enabled;

    /** Ratio at which `color_mid` takes effect; default `0.5`. */
    double mid;

    /** Ratio at which `color_high` takes effect; default `0.75`. */
    double high;

    /** Fill color used while `ratio < mid`. */
    ScColor color_low;

    /** Fill color used while `mid <= ratio < high`. */
    ScColor color_mid;

    /** Fill color used while `ratio >= high`. */
    ScColor color_high;
} ScProgressThresholds;

/**
 * Rendering options for a progress bar.
 */
typedef struct ScProgressBarOpts {
    /** Fill style (block, ascii, line, shaded). */
    ScProgressType type;

    /** Left bracket string; `NULL` = no left cap. */
    const char *left_cap;

    /** Right bracket string; `NULL` = no right cap. */
    const char *right_cap;

    /** Fill color; zero-init = no color. Overridden by thresholds when set. */
    ScColor fill_color;

    /** Empty cell color; zero-init = no color. */
    ScColor empty_color;

    /** Optional threshold-based fill colors. */
    ScProgressThresholds thresholds;

    /** When `true`, append `XX%` after the bar. */
    bool show_percent;

    /** When `true` and `max > 0`, append `(value/max)` after the percent. */
    bool show_value;

    /** Fixed bar width in cells; `0` = derive from `width`. */
    int bar_width;

    /** Total line width in columns; `0` = terminal width. */
    int width;

    /** Fixed label column width; `0` = use natural label width. */
    int label_width;

    /** Style applied to the label text; zero-init = no formatting. */
    ScTextStyle label_style;
} ScProgressBarOpts;

/** Opaque progress bar; build with `sc_progressbar_new`. */
typedef struct ScProgressBar ScProgressBar;


/**
 * Allocates a new progress bar.
 *
 * @param opts  Rendering options.
 * @return      Heap-allocated bar; free with `sc_progressbar_free`.
 */
ScProgressBar *sc_progressbar_new(ScProgressBarOpts opts);

/**
 * Sets or replaces the label printed in front of the bar.
 *
 * @param bar    Bar to update.
 * @param label  New label; copied internally; `NULL` removes the label.
 */
SPARCLI_EXPORT void sc_progressbar_set_label(ScProgressBar *bar, const char *label);

/**
 * Renders one frame of the bar followed by `\\r` so the next call
 * overwrites it in place.
 *
 * @param bar    Bar to render.
 * @param value  Current value; treated as a `0.0`–`1.0` ratio when `max == 0`.
 * @param max    Maximum value; `0` switches to ratio mode.
 */
SPARCLI_EXPORT void sc_progressbar_draw(ScProgressBar *bar, double value, double max);

/**
 * Renders the final bar frame followed by `\\n`, ending the animation.
 *
 * @param bar    Bar to finalize.
 * @param value  Current value; see `sc_progressbar_draw`.
 * @param max    Maximum value; see `sc_progressbar_draw`.
 */
SPARCLI_EXPORT void sc_progressbar_finish(ScProgressBar *bar, double value, double max);

/**
 * Frees `bar` and the owned label string.
 *
 * @param bar  Bar to free; safe to pass `NULL`.
 */
SPARCLI_EXPORT void sc_progressbar_free(ScProgressBar *bar);

SPARCLI_END_DECLS
