#pragma once

#include "core/sparcli_export.h"

#include <stdbool.h>
#include <stddef.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_history.h
 * @brief Input history for text prompts: Up/Down recall + optional
 *        persistence.
 *
 * An `ScHistory` holds previously submitted input lines. Attach it to a text
 * input via `ScTextInputOpts.history` and the widget recalls entries with
 * Up/Down (newest first) and - unless opted out - appends every submitted
 * line automatically. The handle outlives individual prompts, so one history
 * naturally spans a whole REPL session.
 *
 * @code
 * ScHistory *hist = sc_history_new((ScHistoryOpts){
 *     .app = "myapp",   // persisted in ~/.local/state/myapp/history
 * });
 *
 * for (;;) {
 *     char *line = NULL;
 *     ScTextInputOpts opts = { .history = hist, .hide_summary = true };
 *     if (sc_text_input("repl>", &line, opts) != SC_INPUT_OK) { break; }
 *     // ... dispatch(line) ...
 *     free(line);
 * }
 * sc_history_free(hist);   // saves the file
 * @endcode
 */

/** Options for `sc_history_new`. All string fields are copied. */
typedef struct ScHistoryOpts {
    /** Maximum retained entries; `0` = 500. Oldest entries are evicted when
        the cap is exceeded. */
    size_t max_entries;

    /** Application name for XDG persistence: entries are stored in
        `~/.local/state/<app>/history` (created on first use). `NULL` = no
        XDG persistence. */
    const char *app;

    /** Explicit path of the persistence file; overrides `app`. The parent
        directory must exist. `NULL` = use `app` (or stay in-memory). */
    const char *file;

    /** When attached to a text input, do NOT auto-add submitted lines (the
        application then curates the history via `sc_history_add`). */
    bool no_auto_add;

    /** Keep consecutive duplicate lines instead of collapsing them. */
    bool keep_duplicates;
} ScHistoryOpts;

/** Opaque input-history handle; create with `sc_history_new`. */
typedef struct ScHistory ScHistory;

/**
 * Allocates a history.
 *
 * When a persistence target is configured (`opts.file` or `opts.app`),
 * existing entries are loaded immediately; a missing file simply starts
 * empty.
 *
 * @param opts  Configuration; string fields are copied.
 * @return      Heap-allocated handle (free with `sc_history_free`);
 *              `NULL` on allocation failure.
 */
SPARCLI_EXPORT ScHistory *sc_history_new(ScHistoryOpts opts);

/**
 * Appends one line (copied and sanitized).
 *
 * Empty lines, lines containing line breaks, and - unless `keep_duplicates`
 * was set - lines equal to the most recent entry are skipped. When the entry
 * cap is reached, the oldest entry is evicted.
 *
 * @param history  Target history; no-op when `NULL`.
 * @param line     The submitted input line; copied.
 */
SPARCLI_EXPORT void sc_history_add(ScHistory *history, const char *line);

/**
 * Number of retained entries.
 *
 * @param history  History to query; `0` when `NULL`.
 */
SPARCLI_EXPORT size_t sc_history_count(const ScHistory *history);

/**
 * Returns the entry at `index` (`0` = oldest, `count - 1` = newest).
 *
 * @param history  History to read.
 * @param index    Entry position.
 * @return         Borrowed string (valid until the next mutation);
 *                 `NULL` when out of range.
 */
SPARCLI_EXPORT const char *sc_history_get(
    const ScHistory *history, size_t index
);

/**
 * Writes all entries to the configured persistence file (one per line).
 *
 * @param history  History to save.
 * @return         `true` on success; `false` when no file is configured or
 *                 the write fails.
 */
SPARCLI_EXPORT bool sc_history_save(const ScHistory *history);

/**
 * Reloads the entries from the configured persistence file, replacing the
 * current set. Each line is sanitized on load.
 *
 * @param history  History to reload.
 * @return         `true` on success; `false` when no file is configured or
 *                 it cannot be read.
 */
SPARCLI_EXPORT bool sc_history_load(ScHistory *history);

/**
 * Saves the history (when a persistence file is configured) and frees the
 * handle.
 *
 * @param history  History to free; safe to pass `NULL`.
 */
SPARCLI_EXPORT void sc_history_free(ScHistory *history);

SPARCLI_END_DECLS
