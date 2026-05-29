#pragma once

#include "core/sparcli_core.h"
#include "input/sparcli_term.h"

#include <time.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_datepicker.h
 * @brief Interactive month-calendar date picker.
 */

/** Options for `sc_datepicker`. */
typedef struct {
    const char *prompt;     /**< Heading above the calendar; may be `NULL`. */
    int         week_start; /**< 0 = Sunday, 1 = Monday (default). */
    ScColor     accent;     /**< Highlight color for the selected day. */
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
