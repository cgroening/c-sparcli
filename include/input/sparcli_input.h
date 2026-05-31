#pragma once

/**
 * @file sparcli_input.h
 * @brief Sub-umbrella for sparcli's interactive input widgets.
 *
 * Unlike the output side (which writes to the redirectable
 * `sc_output_stream()`), every widget reached through this header drives a
 * real terminal in raw mode and reads decoded keystrokes. Include this for
 * confirm prompts, text/password entry, single/multi selection, the fuzzy
 * finder and the date picker. `sparcli.h` includes it transitively.
 */

#include "input/sparcli_term.h"         // IWYU pragma: export
#include "input/sparcli_shortcut.h"     // IWYU pragma: export
#include "input/sparcli_theme.h"        // IWYU pragma: export
#include "input/sparcli_confirm.h"      // IWYU pragma: export
#include "input/sparcli_text_input.h"   // IWYU pragma: export
#include "input/sparcli_number.h"       // IWYU pragma: export
#include "input/sparcli_textarea.h"     // IWYU pragma: export
#include "input/sparcli_select.h"       // IWYU pragma: export
#include "input/sparcli_fuzzy.h"        // IWYU pragma: export
#include "input/sparcli_datepicker.h"   // IWYU pragma: export
