#pragma once

#include "core/sparcli_core.h"
#include "input/sparcli_term.h"

#include <stddef.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_select.h
 * @brief Single-choice and multi-choice list selection.
 *
 * Built as an opaque handle (like `ScTableData`/`ScList`) because a
 * selection carries a variable number of items and per-run configuration.
 * Set `multi = true` for checkbox-style multi-selection.
 */

/** Options for a selection prompt. */
typedef struct {
    const char  *prompt;         /**< Heading above the list; may be `NULL`. */
    bool         multi;          /**< `true` = multi-select with checkboxes. */
    int          max_visible;    /**< Max rows shown at once; 0 = 10. */
    ScTextStyle  prompt_style;   /**< Style for the heading. */
    ScColor      accent;         /**< Highlight color for the cursor row. */
    ScTextStyle  selected_style; /**< Style of the cursor row; zero-init =
                                      bold in `accent`. */
    const char  *cursor_marker;  /**< Prefix for the cursor row; `NULL` = "‣ ". */
    const char  *marker;         /**< Prefix for other rows;     `NULL` = "  ". */
    const char  *checkbox_on;    /**< Checked box (multi);   `NULL` = "[x] ". */
    const char  *checkbox_off;   /**< Unchecked box (multi); `NULL` = "[ ] ". */
    ScTextStyle  summary_style;  /**< Style of the persistent summary line. */
    bool         hide_summary;   /**< Suppress the post-selection summary line. */
    const char  *hint;           /**< Key-hint footer; `NULL` = sensible default. */
    bool         hide_hint;      /**< Suppress the key-hint footer. */
    ScTextStyle  hint_style;     /**< Style of the footer; zero-init = dim. */
} ScSelectOpts;

/** Opaque selection instance; build with `sc_select_new`. */
typedef struct ScSelect ScSelect;

/**
 * Allocates a new selection prompt.
 *
 * @param opts  Configuration (copied internally).
 * @return      Heap-allocated handle; free with `sc_select_free`.
 */
SPARCLI_EXPORT ScSelect *sc_select_new(ScSelectOpts opts);

/**
 * Appends a selectable item.
 *
 * @param select  Target selection.
 * @param label   Item label; copied internally.
 */
SPARCLI_EXPORT void sc_select_add(ScSelect *select, const char *label);

/**
 * Pre-positions the cursor on `index` (clamped) before `sc_select_run`, so a
 * sensible default is highlighted on first paint. No-op out of range.
 */
SPARCLI_EXPORT void sc_select_set_cursor(ScSelect *select, size_t index);

/**
 * Pre-checks or unchecks `index` (multi-select) before `sc_select_run`, to seed
 * a default set of selected items. No-op out of range.
 */
SPARCLI_EXPORT void sc_select_set_checked(ScSelect *select, size_t index, bool on);

/**
 * Runs the interactive selection.
 *
 * Arrow keys / `j` / `k` move the cursor; for multi-select, Space toggles
 * the current item; Enter confirms; Esc or Ctrl-C cancels.
 *
 * Single-select writes one index. Multi-select writes the checked indices
 * in display order. `*count_io` is in/out: on input it is the capacity of
 * `indices`; on `SC_INPUT_OK` it is set to the number actually written.
 *
 * @param select    Selection to run.
 * @param indices   Caller-provided buffer receiving chosen item indices.
 * @param count_io  In: capacity of `indices`. Out: number written.
 * @return          `SC_INPUT_OK`, `SC_INPUT_CANCELLED`, or `SC_INPUT_ERROR`.
 */
SPARCLI_EXPORT ScInputStatus sc_select_run(
    ScSelect *select, size_t *indices, size_t *count_io
);

/**
 * Frees a selection and all owned item labels.
 *
 * @param select  Selection to free; safe to pass `NULL`.
 */
SPARCLI_EXPORT void sc_select_free(ScSelect *select);

SPARCLI_END_DECLS
