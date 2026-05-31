#pragma once

#include "core/sparcli_core.h"
#include "input/sparcli_term.h"
#include "input/sparcli_shortcut.h"

#include <time.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_datepicker.h
 * @brief Interactive month-calendar date picker.
 */

/**
 * First column of the week. Zero-init selects the default (Monday); Sunday is
 * an explicit value so it stays reachable (a bare `0` cannot mean both "unset"
 * and "Sunday").
 */
typedef enum ScWeekStart {
    SC_WEEK_START_DEFAULT = 0, /**< Same as Monday. */
    SC_WEEK_START_MONDAY = 1,
    SC_WEEK_START_SUNDAY = 2,
} ScWeekStart;

/** Options for `sc_datepicker`. */
typedef struct ScDatePickerOpts {
    /** Heading above the calendar; may be `NULL`. */
    const char *prompt;

    /** First weekday column; zero-init = Monday. */
    ScWeekStart week_start;

    /** Highlight color for the selected day. */
    ScColor accent;

    /** Style of the heading; zero-init = bold. */
    ScTextStyle prompt_style;

    /** Style of the "Month Year" line; zero-init = bold in `accent`. */
    ScTextStyle header_style;

    /** Style of the weekday row; zero-init = dim. */
    ScTextStyle weekday_style;

    /** Style of the selected day; zero-init = bold black-on-`accent`. */
    ScTextStyle selected_style;

    /** Glyph left of the month; `NULL` = "‹". */
    const char *header_prev;

    /** Glyph right of the month; `NULL` = "›". */
    const char *header_next;

    /** Style of the persistent summary line. */
    ScTextStyle summary_style;

    /** Suppress the post-pick summary line. */
    bool hide_summary;

    /** Key-hint footer; `NULL` = sensible default. */
    const char *hint;

    /** Key-hint footer layout: inline (default) / stacked / hidden. */
    ScHintLayout hint_layout;

    /** Key-hint footer placement: top / bottom (default) / left / right. */
    ScHintPosition hint_pos;

    /** Style of the footer; zero-init = dim. */
    ScTextStyle hint_style;

    /** Custom key shortcuts; borrowed, must outlive the call. @see sparcli_shortcut.h */
    const ScShortcut *shortcuts;

    /** Number of entries in `shortcuts`. */
    size_t n_shortcuts;

    /** Optional: receives the fired shortcut id (RETURN mode), else `-1`. */
    int *out_shortcut_id;

    /** Optional rich prompt (mixed styles); overrides the string prompt. Borrowed. */
    const struct ScText *prompt_text;

    /** Parse the string prompt as inline markup. */
    bool prompt_markup;
} ScDatePickerOpts;

/**
 * Prompts the user to pick a date from a month calendar.
 *
 * Arrow keys move by day/week; PageUp/PageDown (or `<`/`>`) change month;
 * Shift+PageUp/PageDown change year; Enter selects; Esc or Ctrl-C cancels.
 * Month/year jumps keep the selected day, clamped to the target month's last
 * valid day (e.g. Jan 31 -> Feb 28).
 *
 * `io` is in/out: its `tm_year`/`tm_mon`/`tm_mday` seed the initial view
 * (a zeroed `struct tm` starts at today). On `SC_INPUT_OK` it is overwritten
 * with the picked date (normalized via `mktime`).
 *
 * @param io    In: initial date. Out: picked date. Must not be `NULL`.
 * @param opts  Rendering options.
 * @return      `SC_INPUT_OK`, `SC_INPUT_CANCELLED`, or `SC_INPUT_ERROR`.
 */
SPARCLI_EXPORT ScInputStatus sc_datepicker(
    struct tm *io, ScDatePickerOpts opts
);

SPARCLI_END_DECLS
