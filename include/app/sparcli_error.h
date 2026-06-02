#pragma once

#include "core/sparcli_export.h"


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_error.h
 * @brief Pretty errors: structured, panel-rendered error reporting.
 *
 * The structured replacement for `fprintf(stderr, "error: …"); exit(1);`
 * in CLI applications: an error carries a message, an optional cause
 * chain, an optional hint and an exit code, and renders as a red alert
 * panel. `sc_error_print` renders without exiting; `sc_die` renders to
 * stderr and terminates the process.
 *
 * No signal handling / crash trapping happens here - this is purely a
 * reporting convention on top of the existing alert/panel renderer.
 */


/** Opaque error builder; create with `sc_error_new`, strings are copied. */
typedef struct ScError ScError;


/**
 * Allocates a new error with the given message.
 *
 * @param message  Human-readable error message (one line works best);
 *                 copied. `NULL` is treated as an empty message.
 * @return         Heap-allocated error; `NULL` on allocation failure.
 *                 Free with `sc_error_free` (or consume with `sc_die`).
 *
 * @code
 * ScError *error = sc_error_new("Config could not be loaded");
 * sc_error_add_cause(error, "file not found: ~/.config/app/config.toml");
 * sc_error_set_hint(error, "Run 'app init' to create a default config");
 * sc_error_set_code(error, 2);
 * sc_die(error);   // renders to stderr, exits with code 2
 * @endcode
 */
SPARCLI_EXPORT ScError *sc_error_new(const char *message);

/**
 * Appends one entry to the error's cause chain (rendered as a dim
 * `caused by:` line, in the order added).
 *
 * @param error  Target error; no-op when `NULL`.
 * @param cause  Cause description; copied. No-op when `NULL`.
 */
SPARCLI_EXPORT void sc_error_add_cause(ScError *error, const char *cause);

/**
 * Sets the hint line (rendered as a `Hint:` block under the causes).
 *
 * @param error  Target error; no-op when `NULL`.
 * @param hint   Suggestion for the user; copied. `NULL` removes the hint.
 */
SPARCLI_EXPORT void sc_error_set_hint(ScError *error, const char *hint);

/**
 * Sets the process exit code used by `sc_die` (default `1`).
 *
 * @param error      Target error; no-op when `NULL`.
 * @param exit_code  Exit code, typically `1`-`125`.
 */
SPARCLI_EXPORT void sc_error_set_code(ScError *error, int exit_code);

/**
 * Returns the error's exit code (default `1`; `1` for `NULL`).
 *
 * @param error  Error to inspect.
 */
SPARCLI_EXPORT int sc_error_code(const ScError *error);

/**
 * Renders the error as a red alert panel to the current output stream.
 *
 * Does not exit and does not free the error; reusable (e.g. render the
 * same error to a log capture and to the terminal).
 *
 * @param error  Error to render; no-op when `NULL`.
 */
SPARCLI_EXPORT void sc_error_print(const ScError *error);

/**
 * Renders the error as a red alert panel to **stderr**, regardless of the
 * current output stream (which is restored afterwards).
 *
 * @param error  Error to render; no-op when `NULL`.
 */
SPARCLI_EXPORT void sc_error_print_stderr(const ScError *error);

/**
 * Frees an error and all owned strings.
 *
 * @param error  Error to free; safe to pass `NULL`.
 */
SPARCLI_EXPORT void sc_error_free(ScError *error);

/**
 * Renders the error to stderr, frees it, and exits the process with the
 * error's exit code.
 *
 * @param error  Error to report; consumed. `NULL` exits with code `1`
 *               without printing.
 */
SPARCLI_EXPORT _Noreturn void sc_die(ScError *error);

/**
 * One-shot convenience: builds an error from `message` and `hint`,
 * renders it to stderr, and exits with `code`.
 *
 * @param code     Process exit code.
 * @param message  Error message; `NULL` = empty.
 * @param hint     Hint line; `NULL` = none.
 *
 * @code
 * if (!config_file) {
 *     sc_die_msg(2, "No config file found",
 *                "Run 'app init' to create one");
 * }
 * @endcode
 */
SPARCLI_EXPORT _Noreturn void sc_die_msg(
    int code, const char *message, const char *hint
);

SPARCLI_END_DECLS
