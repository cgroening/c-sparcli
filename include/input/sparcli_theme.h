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
typedef struct {
    ScColor       accent;         /**< Default highlight color. */
    ScBorderStyle border;         /**< Default box border (text/password boxed mode). */
    ScTextStyle   prompt_style;   /**< Default prompt/heading style. */
    ScTextStyle   selected_style; /**< Default selected/cursor-row style. */
    ScTextStyle   cursor_style;   /**< Default text-cursor cell style. */
    ScTextStyle   count_style;    /**< Default counter style. */
    ScTextStyle   summary_style;  /**< Default summary-line style. */
    ScTextStyle   error_style;    /**< Default validation-error style. */
    ScTextStyle   hint_style;     /**< Default key-hint footer style. */
    const char   *cursor_marker;  /**< Default cursor-row marker. */
    const char   *marker;         /**< Default non-cursor marker. */
    const char   *checkbox_on;    /**< Default checked box (multi-select). */
    const char   *checkbox_off;   /**< Default unchecked box (multi-select). */
    bool          hide_hint;      /**< When true, suppress footers by default. */
} ScInputTheme;

/**
 * Installs the process-wide input theme (copied). Pass `NULL` to clear it and
 * revert to the built-in defaults. Not thread-safe.
 */
SPARCLI_EXPORT void sc_input_set_theme(const ScInputTheme *theme);

/** Returns the current theme (a zeroed struct when none is set). */
SPARCLI_EXPORT ScInputTheme sc_input_theme(void);

SPARCLI_END_DECLS
