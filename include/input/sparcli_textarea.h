#pragma once

#include "core/sparcli_core.h"
#include "input/sparcli_term.h"


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_textarea.h
 * @brief Multi-line text entry.
 *
 * Like `sc_text_input`, but Enter inserts a newline and the value spans
 * multiple lines. Submit with Ctrl-D; Esc or Ctrl-C cancels. Arrow keys move
 * across lines and columns; Home/End jump within the current line.
 */

/** Options for `sc_textarea`. */
typedef struct ScTextareaOpts {
    /** Pre-filled multi-line value; `NULL` = empty. */
    const char *initial;

    /** Dim hint shown while empty; may be `NULL`. */
    const char *placeholder;

    /** Style for the prompt heading. */
    ScTextStyle prompt_style;

    /** Style for the entered text. */
    ScTextStyle value_style;

    /** Cursor cell; zero-init = black-on-white. */
    ScTextStyle cursor_style;

    /** Style of the persistent summary line. */
    ScTextStyle summary_style;

    /** Suppress the post-entry summary line. */
    bool hide_summary;

    /** Key-hint footer; `NULL` = sensible default. */
    const char *hint;

    /** Suppress the key-hint footer. */
    bool hide_hint;

    /** Style of the footer; zero-init = dim. */
    ScTextStyle hint_style;

    /** Render the editor inside a bordered panel (prompt = top title). */
    bool boxed;

    /** Box border (boxed mode); zero-init type = rounded. */
    ScBorderStyle border;

    /** Box width; `0` = full terminal width. */
    int width;
} ScTextareaOpts;

/**
 * Prompts for multi-line text.
 *
 * On `SC_INPUT_OK`, `*out` receives a heap-allocated string (with embedded
 * newlines) the caller must `free()`.
 *
 * @param prompt  Heading shown above the editor. Must not be `NULL`.
 * @param out     Receives the entered text (heap; caller frees).
 * @param opts    Rendering options.
 */
SPARCLI_EXPORT ScInputStatus sc_textarea(
    const char *prompt, char **out, ScTextareaOpts opts
);

SPARCLI_END_DECLS
