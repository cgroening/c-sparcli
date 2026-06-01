#pragma once

#include "core/sparcli_core.h"
#include "input/sparcli_term.h"
#include "input/sparcli_shortcut.h"


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_number.h
 * @brief Numeric input with min/max bounds, step and arrow-key adjustment.
 *
 * Built on the shared line editor: the user can type a number directly
 * (digits, sign, decimal separator) or adjust it with Up/Down by `step`.
 * The value is clamped to `[min, max]` when bounds are given.
 */

/** Options for `sc_number_input`. */
typedef struct ScNumberOpts {
    /** Starting value. */
    double initial;

    /** Start with an empty field instead of the formatted `initial` value
        (type the number directly, no pre-fill to delete). Enter on an empty
        field is ignored, so the prompt never submits "nothing" as `0`. */
    bool start_empty;

    /** Lower bound (used when `max > min`). */
    double min;

    /** Upper bound; `max <= min` = unbounded. */
    double max;

    /** Up/Down increment; `0` = 1. */
    double step;

    /** Fractional digits shown; `0` = integer. */
    int decimals;

    /** Style for the prompt label. */
    ScTextStyle prompt_style;

    /** Style for the value. */
    ScTextStyle value_style;

    /** Cursor cell; zero-init = black-on-white. */
    ScTextStyle cursor_style;

    /** Style of the persistent summary line. */
    ScTextStyle summary_style;

    /** Suppress the post-entry summary line. */
    bool hide_summary;

    /** Key-hint footer; `NULL` = sensible default. */
    const char *hint;

    /** Key-hint footer layout: inline (default) / stacked / hidden. */
    ScHintLayout hint_layout;

    /** Key-hint footer placement: top / bottom (default) / left / right. */
    ScHintPosition hint_pos;

    /** Style of the footer; zero-init = dim. */
    ScTextStyle hint_style;

    /** Render inside a panel (prompt = top title, range/footer below). */
    bool boxed;

    /** Box border (boxed mode); zero-init type = rounded. */
    ScBorderStyle border;

    /** Box width; `0` = full terminal width. */
    int width;

    /** Custom key shortcuts; borrowed, must outlive the call.
        @see sparcli_shortcut.h */
    const ScShortcut *shortcuts;

    /** Number of entries in `shortcuts`. */
    size_t n_shortcuts;

    /** Optional: receives the fired shortcut id (RETURN mode), else `-1`. */
    int *out_shortcut_id;

    /** Optional rich prompt (mixed styles); overrides the string prompt.
        Borrowed. */
    const struct ScText *prompt_text;

    /** Parse the string prompt as inline markup. */
    bool prompt_markup;

    /** Decimal separator for display and input; `0` or `'.'` = period,
        `','` = comma. Both `.` and `,` keystrokes are always accepted and
        shown as the configured separator. */
    char decimal_sep;

    /**
     * Optional: on `SC_INPUT_OK` receives a heap copy of the submitted value
     * as text - exact, never round-tripped through `double`. Always uses
     * `'.'` as decimal separator (machine-readable, e.g. for arbitrary-
     * precision decimal types) and reflects clamping to `[min, max]`.
     * Caller frees with `free()`. `NULL` = not requested.
     */
    char **out_text;
} ScNumberOpts;

/**
 * Prompts for a number.
 *
 * Type digits/sign/decimal separator to edit; Up/Down adjust by `step`;
 * Enter submits; Esc or Ctrl-C cancels. Enter on an empty field is ignored
 * (clear the field with Ctrl-U, then Enter does nothing until a value is
 * typed). On `SC_INPUT_OK`, `*out` receives the parsed value, clamped to
 * `[min, max]` when bounded; when `opts.out_text` is set it additionally
 * receives the exact value as a heap string (see `ScNumberOpts.out_text`).
 *
 * @param prompt  Label shown before the field. Must not be `NULL`.
 * @param out     Receives the chosen value.
 * @param opts    Rendering and range options.
 */
SPARCLI_EXPORT ScInputStatus sc_number_input(
    const char *prompt, double *out, ScNumberOpts opts
);

SPARCLI_END_DECLS
