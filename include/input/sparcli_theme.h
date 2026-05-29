#pragma once

#include "core/sparcli_core.h"


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_theme.h
 * @brief Process-wide default styling for input widgets.
 *
 * Set a theme once and every input widget inherits it for any option the
 * caller leaves zero-initialized. Per-call options always win over the theme,
 * and the theme in turn wins over the built-in defaults. This removes the
 * boilerplate of repeating accent colors, markers and footer styles on every
 * prompt.
 */

/** Shared default styling. Any zero-init field falls through to the built-in default. */
typedef struct ScInputTheme {
    /** Default highlight color. */
    ScColor accent;

    /** Default box border (text/password boxed mode). */
    ScBorderStyle border;

    /** Default prompt/heading style. */
    ScTextStyle prompt_style;

    /** Default selected/cursor-row style. */
    ScTextStyle selected_style;

    /** Default text-cursor cell style. */
    ScTextStyle cursor_style;

    /** Default counter style. */
    ScTextStyle count_style;

    /** Default summary-line style. */
    ScTextStyle summary_style;

    /** Default validation-error style. */
    ScTextStyle error_style;

    /** Default key-hint footer style. */
    ScTextStyle hint_style;

    /** Default cursor-row marker. */
    const char *cursor_marker;

    /** Default non-cursor marker. */
    const char *marker;

    /** Default checked box (multi-select). */
    const char *checkbox_on;

    /** Default unchecked box (multi-select). */
    const char *checkbox_off;

    /** When true, suppress footers by default. */
    bool hide_hint;
} ScInputTheme;

/**
 * Installs the process-wide input theme (copied). Pass `NULL` to clear it and
 * revert to the built-in defaults. Not thread-safe.
 */
SPARCLI_EXPORT void sc_input_set_theme(const ScInputTheme *theme);

/** Returns the current theme (a zeroed struct when none is set). */
SPARCLI_EXPORT ScInputTheme sc_input_theme(void);

SPARCLI_END_DECLS
