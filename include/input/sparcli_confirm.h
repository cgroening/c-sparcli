#pragma once

#include "core/sparcli_core.h"
#include "input/sparcli_term.h"


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
typedef struct {
    bool        default_yes;      /**< Initial selection (`false` = No). */
    const char *yes_label;        /**< Label for the affirmative; `NULL` = "Yes". */
    const char *no_label;         /**< Label for the negative;    `NULL` = "No". */
    ScTextStyle question_style;   /**< Style for the question text. */
    ScColor     accent;           /**< Highlight color of the selected option. */
    ScTextStyle selected_style;   /**< Style of the selected option; zero-init =
                                       bold black-on-`accent`. */
    ScTextStyle unselected_style; /**< Style of the unselected option; zero-init = dim. */
    ScTextStyle summary_style;    /**< Style of the persistent summary line. */
    bool        hide_summary;     /**< Suppress the post-confirm summary line. */
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
