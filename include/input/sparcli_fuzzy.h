#pragma once

#include "core/sparcli_core.h"
#include "output/sparcli_table.h"
#include "input/sparcli_term.h"

#include <stddef.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_fuzzy.h
 * @brief Incremental fuzzy finder with an optional table view.
 *
 * The user types a query; items are filtered/ranked by subsequence match
 * against their primary field and the result set is re-rendered on every
 * keystroke. Results render either as a simple list (default) or, when
 * `table = true` and columns are supplied, as a sparcli table.
 */

/** Options for a fuzzy finder. */
typedef struct ScFuzzyOpts {
    /** Search-field label; `NULL` = "Search". */
    const char *prompt;

    /** Max result rows shown; `0` = 10. */
    int max_visible;

    /** Highlight color for the cursor row. */
    ScColor accent;

    /** Render results as a table. */
    bool table;

    /** Column headers (table view). */
    const char *const *headers;

    /** Number of columns (table view). */
    size_t n_cols;

    /** Search label; zero-init = bold in `accent`. */
    ScTextStyle prompt_style;

    /** List cursor row; zero-init = bold in `accent`. */
    ScTextStyle selected_style;

    /** Query cursor cell; zero-init = black-on-white. */
    ScTextStyle cursor_style;

    /** "(n/total)" counter; zero-init = dim. */
    ScTextStyle counter_style;

    /** List cursor prefix; `NULL` = "‣ ". */
    const char *cursor_marker;

    /** Other-row prefix; `NULL` = "  ". */
    const char *marker;

    /**
     * Table-view opts (table mode); zero-init = single border + bold header.
     * The cursor row is highlighted with `accent` regardless.
     */
    ScTableOpts table_opts;

    /** Style of the persistent summary line. */
    ScTextStyle summary_style;

    /** Suppress the post-pick summary line. */
    bool hide_summary;

    /** Key-hint footer; `NULL` = default. */
    const char *hint;

    /** Suppress the key-hint footer. */
    bool hide_hint;

    /** Style of the footer; zero-init = dim. */
    ScTextStyle hint_style;
} ScFuzzyOpts;

/** Opaque fuzzy-finder instance; build with `sc_fuzzy_new`. */
typedef struct ScFuzzy ScFuzzy;

/**
 * Computes a fuzzy subsequence match of `pattern` against `str`.
 *
 * Case-insensitive. Pure function (no terminal dependency), exposed for
 * unit testing and reuse. A higher score is a better match; contiguous
 * and word-start matches score higher.
 *
 * @param pattern  Query string (may be empty → always matches, score 0).
 * @param str      Candidate string.
 * @param score    Optional out-param receiving the match score.
 * @return         `true` when every char of `pattern` occurs in order.
 */
SPARCLI_EXPORT bool sc_fuzzy_match(
    const char *pattern, const char *str, int *score
);

/**
 * Allocates a new fuzzy finder.
 *
 * @param opts  Configuration (copied internally).
 * @return      Heap-allocated handle; free with `sc_fuzzy_free`.
 */
SPARCLI_EXPORT ScFuzzy *sc_fuzzy_new(ScFuzzyOpts opts);

/**
 * Adds a single-field item (list view, or a one-column table).
 *
 * @param fuzzy  Target finder.
 * @param label  Item text used for both matching and display; copied.
 */
SPARCLI_EXPORT void sc_fuzzy_add(ScFuzzy *fuzzy, const char *label);

/**
 * Adds a multi-field row (table view). `fields[0]` is used for matching.
 *
 * @param fuzzy   Target finder.
 * @param fields  Array of `n` column strings; copied internally.
 * @param n       Number of fields.
 */
SPARCLI_EXPORT void sc_fuzzy_add_row(
    ScFuzzy *fuzzy, const char *const *fields, size_t n
);

/**
 * Runs the interactive finder.
 *
 * Typing edits the query; arrow keys / Ctrl-N / Ctrl-P move the cursor;
 * Enter selects the highlighted row; Esc or Ctrl-C cancels.
 *
 * @param fuzzy      Finder to run.
 * @param out_index  Receives the chosen item's original add-order index.
 * @return           `SC_INPUT_OK`, `SC_INPUT_CANCELLED`, or `SC_INPUT_ERROR`.
 */
SPARCLI_EXPORT ScInputStatus sc_fuzzy_run(ScFuzzy *fuzzy, size_t *out_index);

/**
 * Frees a finder and all owned rows.
 *
 * @param fuzzy  Finder to free; safe to pass `NULL`.
 */
SPARCLI_EXPORT void sc_fuzzy_free(ScFuzzy *fuzzy);

SPARCLI_END_DECLS
