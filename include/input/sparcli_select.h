#pragma once

#include "core/sparcli_core.h"
#include "input/sparcli_term.h"
#include "input/sparcli_shortcut.h"

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
typedef struct ScSelectOpts {
    /** Heading above the list; may be `NULL`. */
    const char *prompt;

    /** `true` = multi-select with checkboxes. */
    bool multi;

    /** Disable cursor cycling. By default the cursor cycles around the ends: Up
        on the first row jumps to the last and Down on the last jumps to the
        first; set this to stop at the ends instead. */
    bool no_cycle;

    /** Max rows shown at once; `0` = 10. */
    int max_visible;

    /** Style for the heading. */
    ScTextStyle prompt_style;

    /** Highlight color for the cursor row. */
    ScColor accent;

    /** Style of the cursor row; zero-init = bold in `accent`. */
    ScTextStyle selected_style;

    /** Prefix for the cursor row; `NULL` = "‣ ". */
    const char *cursor_marker;

    /** Prefix for other rows; `NULL` = "  ". */
    const char *marker;

    /** Checked box (multi); `NULL` = "[x] ". */
    const char *checkbox_on;

    /** Unchecked box (multi); `NULL` = "[ ] ". */
    const char *checkbox_off;

    /** Optional frame: render the list inside a panel with a border, content
        background, inner padding and outer margin. Zero-init = inline (no
        frame). The prompt stays inside the frame; the key-hint footer sits
        below it. @see ScBoxStyle */
    ScBoxStyle box;

    /** Style of the persistent summary line. */
    ScTextStyle summary_style;

    /** Suppress the post-selection summary line. */
    bool hide_summary;

    /** Key-hint footer; `NULL` = sensible default. */
    const char *hint;

    /** Key-hint footer layout: inline (default) / stacked / hidden. */
    ScHintLayout hint_layout;

    /** Key-hint footer placement: top / bottom (default) / left / right. */
    ScHintPosition hint_pos;

    /** Style of the footer; zero-init = dim. */
    ScTextStyle hint_style;

    /** Custom key shortcuts; borrowed, must outlive the run.
        @see sparcli_shortcut.h */
    const ScShortcut *shortcuts;

    /** Number of entries in `shortcuts`. */
    size_t n_shortcuts;

    /** Optional: receives the fired shortcut id (RETURN mode), else `-1`. */
    int *out_shortcut_id;

    /** Optional rich prompt (mixed styles); overrides the string prompt.
        Borrowed. */
    const struct ScText *prompt_text;

    /** Parse the string prompt as inline markup. */
    bool prompt_markup;
} ScSelectOpts;

/** Opaque selection instance; build with `sc_select_new`. */
typedef struct ScSelect ScSelect;

/**
 * Allocates a new selection prompt.
 *
 * @param opts  Configuration. Copied internally, including the string fields
 *              (`prompt`, markers, checkbox glyphs, `hint`); only the fields
 *              documented as borrowed (`shortcuts`, `prompt_text`) must
 *              outlive the prompt.
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
SPARCLI_EXPORT void sc_select_set_checked(
    ScSelect *select, size_t index, bool on
);

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
 * Returns the index of the currently highlighted item.
 *
 * Intended for use from a `SC_SHORTCUT_CALLBACK` callback, which receives the
 * `ScSelect*` via its `user` pointer and can query the live cursor. Returns 0
 * for a `NULL` selection.
 */
SPARCLI_EXPORT size_t sc_select_cursor(const ScSelect *select);

/**
 * Returns the current label of the item at `index` (current order), or `NULL`
 * when out of range. The pointer is owned by the selection - copy it if you
 * need it past the next mutation.
 */
SPARCLI_EXPORT const char *sc_select_label(
    const ScSelect *select, size_t index
);

/**
 * Replaces the label of the item at `index` (the new string is copied). No-op
 * when `index` is out of range or on allocation failure (the old label is
 * kept). Pair with `sc_select_set_cursor` to edit an item between runs, e.g.
 * from a RETURN-mode shortcut that opens a text prompt.
 */
SPARCLI_EXPORT void sc_select_set_label(
    ScSelect *select, size_t index, const char *label
);

/**
 * Removes the item at `index` (0-based, current order), freeing its label and
 * shifting the rest down. Intended for a `SC_SHORTCUT_CALLBACK` to delete the
 * highlighted item live; the cursor is clamped and kept in view. No-op when
 * `index` is out of range. Removing the last item leaves an empty list (a
 * subsequent Enter then selects nothing).
 */
SPARCLI_EXPORT void sc_select_remove(ScSelect *select, size_t index);

/**
 * Frees a selection and all owned item labels.
 *
 * @param select  Selection to free; safe to pass `NULL`.
 */
SPARCLI_EXPORT void sc_select_free(ScSelect *select);

SPARCLI_END_DECLS
