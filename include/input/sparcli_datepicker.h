#pragma once

#include "core/sparcli_core.h"
#include "input/sparcli_term.h"

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
typedef enum {
    SC_WEEK_START_DEFAULT = 0, /**< Same as Monday. */
    SC_WEEK_START_MONDAY  = 1,
    SC_WEEK_START_SUNDAY  = 2,
} ScWeekStart;

/** Options for `sc_datepicker`. */
typedef struct {
    const char *prompt;         /**< Heading above the calendar; may be `NULL`. */
    ScWeekStart week_start;     /**< First weekday column; zero-init = Monday. */
    ScColor     accent;         /**< Highlight color for the selected day. */
    ScTextStyle prompt_style;   /**< Style of the heading; zero-init = bold. */
    ScTextStyle header_style;   /**< Style of the "Month Year" line; zero-init =
                                     bold in `accent`. */
    ScTextStyle weekday_style;  /**< Style of the weekday row; zero-init = dim. */
    ScTextStyle selected_style; /**< Style of the selected day; zero-init =
                                     bold black-on-`accent`. */
    const char *header_prev;    /**< Glyph left of the month; `NULL` = "‹". */
    const char *header_next;    /**< Glyph right of the month; `NULL` = "›". */
    ScTextStyle summary_style;  /**< Style of the persistent summary line. */
    bool        hide_summary;   /**< Suppress the post-pick summary line. */
    const char *hint;           /**< Key-hint footer; `NULL` = sensible default. */
    bool        hide_hint;      /**< Suppress the key-hint footer. */
    ScTextStyle hint_style;     /**< Style of the footer; zero-init = dim. */
} ScDatePickerOpts;

/**
 * Prompts the user to pick a date from a month calendar.
 *
 * Arrow keys move by day/week; PageUp/PageDown (or `<`/`>`) change month;
 * Enter selects; Esc or Ctrl-C cancels.
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
