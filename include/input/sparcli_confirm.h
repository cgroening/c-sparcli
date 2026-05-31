#pragma once

#include "core/sparcli_core.h"
#include "input/sparcli_term.h"
#include "input/sparcli_shortcut.h"

#include <stddef.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_confirm.h
 * @brief Yes/No confirmation prompt.
 */

/**
 * Options for `sc_confirm`.
 *
 * Zero-initialized opts give a sensible prompt: "No" preselected, default
 * labels, accent in the terminal's default color.
 */
typedef struct ScConfirmOpts {
    /** Initial selection (`false` = No). */
    bool default_yes;

    /** Label for the affirmative; `NULL` = "Yes". */
    const char *yes_label;

    /** Label for the negative; `NULL` = "No". */
    const char *no_label;

    /** Style for the question text. */
    ScTextStyle prompt_style;

    /** Highlight color of the selected option. */
    ScColor accent;

    /** Style of the selected option; zero-init = bold black-on-`accent`. */
    ScTextStyle selected_style;

    /** Style of the unselected option; zero-init = dim. */
    ScTextStyle unselected_style;

    /** Style of the persistent summary line. */
    ScTextStyle summary_style;

    /** Suppress the post-confirm summary line. */
    bool hide_summary;

    /** Key-hint footer; `NULL` = sensible default. */
    const char *hint;

    /** Key-hint footer layout: inline (default) / stacked / hidden. */
    ScHintLayout hint_layout;

    /** Key-hint footer placement: top / bottom (default) / left / right. */
    ScHintPosition hint_pos;

    /** Style of the footer; zero-init = dim. */
    ScTextStyle hint_style;

    /** Custom key shortcuts; borrowed, must outlive the call.
        @see sparcli_shortcut.h */
    const ScShortcut *shortcuts;

    /** Number of entries in `shortcuts`. */
    size_t n_shortcuts;

    /** Optional: receives the fired shortcut id (RETURN mode), else `-1`. */
    int *out_shortcut_id;

    /** Optional rich question (mixed styles); overrides the string + style.
        Borrowed. */
    const struct ScText *prompt_text;

    /** Parse the question string as inline markup, e.g. "[bold]Delete?[/]". */
    bool prompt_markup;
} ScConfirmOpts;

/**
 * Prompts the user with a Yes/No question.
 *
 * Arrow keys / Tab / `h` / `l` move the selection; `y`/`n` pick directly;
 * Enter confirms; Esc or Ctrl-C cancels.
 *
 * @param question  Prompt text. Must not be `NULL`.
 * @param out       Receives the choice on `SC_INPUT_OK`; untouched otherwise.
 * @param opts      Rendering options.
 * @return          `SC_INPUT_OK`, `SC_INPUT_CANCELLED`, or `SC_INPUT_ERROR`.
 */
SPARCLI_EXPORT ScInputStatus sc_confirm(
    const char *question, bool *out, ScConfirmOpts opts
);

SPARCLI_END_DECLS
