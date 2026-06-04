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

    /** Optional frame: render inside a panel (prompt = top title, range on the
        bottom border) with a border, content background, inner padding and
        outer margin. Zero-init = inline. `box.width` is the box width (`0` =
        full terminal width). @see ScBoxStyle */
    ScBoxStyle box;

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

    /**
     * Enable calculator mode: typing `=` as the first character starts an
     * arithmetic expression (e.g. `=1,5+2*3`; see `sc_calc_eval` for the
     * syntax). A dim live preview shows the result while typing; Enter
     * replaces the expression with the result (still editable), a second
     * Enter submits it. An invalid expression shows an error line and
     * keeps the prompt open.
     *
     * Precision: the field displays the result rounded to `decimals`, but
     * the submitted value (`*out` / `out_text`) keeps the full-precision
     * result - a deliberate exception to the "displayed text == submitted
     * value" rule (`*out` and `*out_text` still always agree). See
     * `calc_store_rounded` / `calc_show_precise` to change this.
     *
     * While a full-precision result is pending and differs from the rounded
     * display, a dim ` = <value>` indicator is shown next to the field.
     * Editing the field discards the pending result; when that loses
     * precision, a warning line appears (see `calc_warn_text` /
     * `calc_warn_style`) - from then on the displayed text is what gets
     * submitted, exactly like a typed number.
     */
    bool calculator;

    /**
     * Submit the displayed value instead of the full-precision result.
     * With the default rounded display this stores the value rounded to
     * `decimals`, restoring "displayed == submitted".
     */
    bool calc_store_rounded;

    /**
     * Display calculator results in full precision instead of rounded to
     * `decimals`.
     */
    bool calc_show_precise;

    /** Style of the invalid-expression error line; zero-init = red. */
    ScTextStyle error_style;

    /**
     * Text of the warning shown when an edit discards a pending full-
     * precision calculator result that differs from the displayed value
     * (precision is actually lost): from then on the displayed text is
     * what gets submitted. `NULL` = built-in English default.
     */
    const char *calc_warn_text;

    /** Style of the discarded-result warning line; zero-init = yellow. */
    ScTextStyle calc_warn_style;
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

/**
 * Evaluates a basic arithmetic expression.
 *
 * Supports `+ - * /`, parentheses, a single unary minus per operand and
 * numbers with either `.` or `,` as decimal separator (e.g. `"1,5+2*3"`,
 * `"2*-3"`). Whitespace is ignored; `1++2` and `--3` are rejected.
 * Pure function (no terminal dependency), exposed for unit testing and
 * reuse; this is the evaluator behind the number input's calculator mode.
 *
 * @param expr    Expression text (without a leading `=`).
 * @param result  Receives the value on success; untouched on failure.
 * @return        `true` when the whole expression parses to a finite value;
 *                `false` on syntax errors, division by zero or overflow.
 */
SPARCLI_EXPORT bool sc_calc_eval(const char *expr, double *result);

SPARCLI_END_DECLS
