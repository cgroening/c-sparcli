#pragma once

#include "core/sparcli_core.h"

SPARCLI_BEGIN_DECLS


/**
 * Animation style of the spinner character.
 */
typedef enum ScSpinnerType {
    SC_SPINNER_BRAILLE, /**< `⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏` (10 frames, Braille dots) */
    SC_SPINNER_PIPE,    /**< `|/-\` (4 frames, ASCII) */
    SC_SPINNER_DOTS,    /**< `⣾⣽⣻⢿⡿⣟⣯⣷` (8 frames, dense Braille) */
    SC_SPINNER_ARROW,   /**< `←↖↑↗→↘↓↙` (8 frames, rotating arrow) */
} ScSpinnerType;

/**
 * Rendering options for a spinner.
 */
typedef struct ScSpinnerOpts {
    /** Animation frames used for the spinner character. */
    ScSpinnerType type;

    /** Color applied to the spinner character; zero-init = no color. */
    ScColor color;

    /** Style applied to the label text; zero-init = no formatting. */
    ScTextStyle label_style;
} ScSpinnerOpts;

/** Opaque spinner instance; build with `sc_spinner_new`. */
typedef struct ScSpinner ScSpinner;


/**
 * Allocates a new spinner.
 *
 * @param label  Initial label printed next to the spinner; may be `NULL`.
 * @param opts   Rendering options.
 * @return       Heap-allocated spinner; free with `sc_spinner_free`.
 */
ScSpinner *sc_spinner_new(const char *label, ScSpinnerOpts opts);

/**
 * Sets or replaces the label printed next to the spinner.
 *
 * @param spinner  Spinner to update.
 * @param label    New label; copied internally; `NULL` removes the label.
 */
SPARCLI_EXPORT void sc_spinner_set_label(ScSpinner *spinner, const char *label);

/**
 * Advances to the next animation frame and prints `frame label\r`.
 *
 * Pads to the terminal width so a shorter new label cleanly overwrites a
 * longer previous one. Always calls `fflush(stdout)`.
 *
 * @param spinner  Spinner to advance.
 */
SPARCLI_EXPORT void sc_spinner_tick(ScSpinner *spinner);

/**
 * Clears the spinner line and prints a final status: `✔ label` (green) on
 * success, `✖ label` (red) on failure, followed by `\n`.
 *
 * @param spinner  Spinner being finished.
 * @param success  `true` = print `✔` in green; `false` = print `✖` in red.
 * @param label    Optional label printed after the symbol; may be `NULL`.
 */
SPARCLI_EXPORT void sc_spinner_finish(
    ScSpinner *spinner, bool success, const char *label
);

/**
 * Frees `spinner` and the owned label string.
 *
 * @param spinner  Spinner to free; safe to pass `NULL`.
 */
SPARCLI_EXPORT void sc_spinner_free(ScSpinner *spinner);

SPARCLI_END_DECLS
