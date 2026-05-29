#pragma once

#include "core/sparcli_core.h"
#include "input/sparcli_term.h"


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_number.h
 * @brief Numeric input with min/max bounds, step and arrow-key adjustment.
 *
 * Built on the shared line editor: the user can type a number directly
 * (digits, sign, decimal point) or adjust it with Up/Down by `step`. The
 * value is clamped to `[min, max]` when bounds are given.
 */

/** Options for `sc_number_input`. */
typedef struct {
    double      initial;       /**< Starting value. */
    double      min;           /**< Lower bound (used when `max > min`). */
    double      max;           /**< Upper bound; `max <= min` = unbounded. */
    double      step;          /**< Up/Down increment; `0` = 1. */
    int         decimals;      /**< Fractional digits shown; `0` = integer. */
    ScTextStyle prompt_style;  /**< Style for the prompt label. */
    ScTextStyle value_style;   /**< Style for the value. */
    ScTextStyle cursor_style;  /**< Cursor cell; zero-init = black-on-white. */
    ScTextStyle summary_style; /**< Style of the persistent summary line. */
    bool        hide_summary;  /**< Suppress the post-entry summary line. */
    const char *hint;          /**< Key-hint footer; `NULL` = sensible default. */
    bool        hide_hint;     /**< Suppress the key-hint footer. */
    ScTextStyle hint_style;    /**< Style of the footer; zero-init = dim. */
} ScNumberOpts;

/**
 * Prompts for a number.
 *
 * Type digits/sign/`.` to edit; Up/Down adjust by `step`; Enter submits;
 * Esc or Ctrl-C cancels. On `SC_INPUT_OK`, `*out` receives the parsed value,
 * clamped to `[min, max]` when bounded.
 *
 * @param prompt  Label shown before the field. Must not be `NULL`.
 * @param out     Receives the chosen value.
 * @param opts    Rendering and range options.
 */
SPARCLI_EXPORT ScInputStatus sc_number_input(
    const char *prompt, double *out, ScNumberOpts opts
);

SPARCLI_END_DECLS
