#pragma once

#include "core/sparcli_core.h"
#include "output/sparcli_table.h"
#include "input/sparcli_term.h"
#include "input/sparcli_shortcut.h"

#include <stddef.h>
#include <stdint.h>


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

/**
 * Result ordering of the filtered list. With section headers the order is
 * applied *within* each section/group; without sections it is global.
 */
typedef enum ScFuzzyOrder {
    SC_FUZZY_ORDER_SCORE = 0,  /**< match score desc, then add order (default). */
    SC_FUZZY_ORDER_INSERTION,  /**< stable add order (e.g. tasks by time). */
    SC_FUZZY_ORDER_COLUMN,     /**< by `fields[order_column]` (case-insensitive). */
} ScFuzzyOrder;

/** Options for a fuzzy finder. */
typedef struct ScFuzzyOpts {
    /** Search-field label; `NULL` = "Search". */
    const char *prompt;

    /** Max result rows shown; `0` = 10. */
    int max_visible;

    /** Disable cursor cycling. By default the cursor cycles around the ends of
        the result list (Up on the first match jumps to the last, and back);
        set this to stop at the ends instead. */
    bool no_cycle;

    /** Highlight color for the cursor row. */
    ScColor accent;

    /** Render results as a table. */
    bool table;

    /** Column headers (table view). */
    const char *const *headers;

    /** Number of columns (table view). */
    size_t n_cols;

    /**
     * Bitmask selecting which columns the query searches (bit `c` = column
     * `c`). `0` (the default) searches all columns. Table view only - the list
     * view always searches its single label. A row matches when the query
     * matches any selected column; its rank uses the best-scoring column.
     */
    uint64_t search_columns;

    /** Search label; zero-init = bold in `accent`. */
    ScTextStyle prompt_style;

    /**
     * Cursor-row style. List view: zero-init = bold in `accent`. Table view:
     * applied as the cursor row's text style over the accent background
     * (e.g. `.fg = black` for dark text on the highlight); zero-init = the
     * terminal default.
     */
    ScTextStyle selected_style;

    /** Query cursor cell; zero-init = black-on-white. */
    ScTextStyle cursor_style;

    /** "(n/total)" counter; zero-init = dim. */
    ScTextStyle counter_style;

    /** List cursor prefix; `NULL` = "‣ ". */
    const char *cursor_marker;

    /** Other-row prefix; `NULL` = "  ". */
    const char *marker;

    /** Optional frame: render the finder (query + results) inside a panel with
        a border, content background, inner padding and outer margin. Zero-init
        = inline (no frame). The key-hint footer sits below the frame.
        @see ScBoxStyle */
    ScBoxStyle box;

    /**
     * Table-view opts (table mode); zero-init = single border + bold header.
     * The cursor row is highlighted with `accent` by default; set
     * `selected_style.bg` to override the cursor-row background.
     */
    ScTableOpts table_opts;

    /** Style of the persistent summary line. */
    ScTextStyle summary_style;

    /** Suppress the post-pick summary line. */
    bool hide_summary;

    /** Key-hint footer; `NULL` = default. */
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

    /* ---- Multi-select (checkbox) ---- */

    /** `true` = multi-select: Space toggles, run via `sc_fuzzy_run_multi`. */
    bool multi;

    /** Checked box glyph (multi); `NULL` = "[x] ". */
    const char *checkbox_on;

    /** Unchecked box glyph (multi); `NULL` = "[ ] ". */
    const char *checkbox_off;

    /** Table view: render the checkbox as its own leading column instead of a
        glyph prefixed to the first data column. */
    bool checkbox_column;

    /** Key that toggles all selectable rows on/off (multi only). Zero-init
        (`chord.key == 0`) disables it. Build with `sc_key_ctrl('a')` etc. */
    ScKeyChord toggle_all_key;

    /** Key that toggles all selectable rows of the cursor's section on/off
        (multi only). Zero-init disables it. */
    ScKeyChord toggle_section_key;

    /* ---- Sections / disabled / empty ---- */

    /** Style of section-header rows; zero-init = dim + bold. */
    ScTextStyle section_style;

    /** Append the matched-row count to each section header, e.g. "Monday (3)". */
    bool section_counts;

    /** Style of disabled (greyed, non-selectable) rows; zero-init = dim. */
    ScTextStyle disabled_style;

    /** Text shown when no row matches the query; `NULL` = nothing. */
    const char *empty_text;

    /** Style of `empty_text`; zero-init = dim. */
    ScTextStyle empty_style;

    /* ---- Ordering / seed ---- */

    /** Result order (see `ScFuzzyOrder`); applied within sections when present. */
    ScFuzzyOrder order;

    /** Column index for `SC_FUZZY_ORDER_COLUMN`. */
    size_t order_column;

    /** Sort descending for COLUMN / INSERTION order. */
    bool order_desc;

    /** Pre-fill the query field on the next run; `NULL` = empty. */
    const char *initial_query;

    /* ---- Modal normal/insert mode (vim-style; opt-in) ---- */

    /**
     * Enable a modal normal/insert mode. Off by default: every key types into
     * the query field as usual. When on, the finder has a **normal** mode
     * (bare-letter shortcuts fire, `j`/`k`/`g`/`G` navigate) and an **insert**
     * mode (keys edit the query). Toggle with `i` (→ insert) and `Esc`
     * (→ normal); `Esc` in normal mode cancels. The active mode is shown in the
     * query line (color + badge).
     */
    bool modal;

    /** Modal: start in insert mode instead of normal mode (the default). */
    bool start_in_insert;

    /** Chord that enters insert mode (normal mode); zero-init = `i`. */
    ScKeyChord insert_key;

    /** Chord that leaves insert mode / cancels in normal mode;
        zero-init = `Esc`. */
    ScKeyChord normal_key;

    /** Normal-mode chord that clears the query field; zero-init = disabled. */
    ScKeyChord clear_key;

    /** Hide the NORMAL/INSERT badge (the field is still tinted per mode). */
    bool hide_mode_badge;

    /** Normal-mode badge text; `NULL` = "NORMAL". */
    const char *normal_label;

    /** Insert-mode badge text; `NULL` = "INSERT". */
    const char *insert_label;

    /** Badge + query-field style in normal mode; zero-init = bold white on
        blue. */
    ScTextStyle mode_normal_style;

    /** Badge + query-field style in insert mode; zero-init = bold black on
        green. */
    ScTextStyle mode_insert_style;

    /**
     * Table view: bitmask of display columns (bit `c` = column `c`) that stretch
     * to fill the box width when `box` has a bounded width
     * (`SC_WIDTH_FULL`/`FIXED`, or `CONTENT` with a `max_width`). The surplus is
     * split across the selected columns; the rest stay content-sized. `0` (the
     * default) keeps the table content-sized inside the frame. The list view
     * already fills a bounded box, so this is table-view only.
     */
    uint64_t stretch_columns;

    /**
     * Cap the finder's total height (in terminal rows) so it never overflows a
     * limited region; the result list then scrolls within the remaining space
     * (the existing `↑ first–last/total ↓` viewport). `0` (default) auto-fits to
     * the terminal height. Set it to the rows available below a header when the
     * finder runs inside a live dashboard's reserved region (`prompt_rows`).
     * The visible row count is `max_height` minus the query line, the scroll
     * indicator, the box frame and the hint footer.
     */
    int max_height;

    /**
     * Opt out of the right-edge scrollbar. By default a vertical scrollbar
     * (track + thumb) is drawn to the right of the results whenever the list
     * scrolls (more matches than fit), alongside the `↑ first–last/total ↓`
     * text indicator. Set this to suppress the bar and keep only the text line.
     */
    bool no_scrollbar;
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
 * @param opts  Configuration. Copied internally, including the string fields
 *              (`prompt`, markers, `hint`, `headers`); only the fields
 *              documented as borrowed (`shortcuts`, `prompt_text`,
 *              `table_opts` strings) must outlive the finder.
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
 * Adds a non-selectable **section header** row (e.g. a day in a todo list).
 * Headers are shown only when their group has at least one matching row, are
 * skipped by the cursor, and group the rows that follow until the next header.
 *
 * @param fuzzy  Target finder.
 * @param title  Header text; copied internally.
 */
SPARCLI_EXPORT void sc_fuzzy_add_section(ScFuzzy *fuzzy, const char *title);

/**
 * Adds a single-field item with a base text style (whole-cell color/attributes).
 * The query-match highlight (bold + underline) is overlaid on top of `style`.
 */
SPARCLI_EXPORT void sc_fuzzy_add_styled(
    ScFuzzy *fuzzy, const char *label, ScTextStyle style
);

/**
 * Adds a multi-field row with an optional per-cell base style. `styles` (if not
 * `NULL`) holds `n` styles applied as each cell's base; the match highlight is
 * overlaid. `styles` is borrowed for the call (copied internally).
 */
SPARCLI_EXPORT void sc_fuzzy_add_row_styled(
    ScFuzzy *fuzzy, const char *const *fields,
    const ScTextStyle *styles, size_t n
);

/**
 * Adds a multi-field row whose cells are full `ScText` (arbitrary multi-color
 * spans). The match/display key for each cell is the flattened span text, so
 * matching still works, but the per-character match highlight is not drawn in
 * rich cells. `cells` entries are deep-copied; the caller keeps ownership.
 */
SPARCLI_EXPORT void sc_fuzzy_add_row_rich(
    ScFuzzy *fuzzy, ScText *const *cells, size_t n
);

/** Marks the row at `index` (add order) disabled (greyed, non-selectable). */
SPARCLI_EXPORT void sc_fuzzy_set_disabled(
    ScFuzzy *fuzzy, size_t index, bool on
);

/** Sets a stable caller id on the row at `index` (add order). */
SPARCLI_EXPORT void sc_fuzzy_set_id(ScFuzzy *fuzzy, size_t index, uint64_t id);

/** Returns the stable id of the row at `index` (add order), or 0. */
SPARCLI_EXPORT uint64_t sc_fuzzy_id_at(const ScFuzzy *fuzzy, size_t index);

/** Returns the stable id of the currently highlighted row, or 0. */
SPARCLI_EXPORT uint64_t sc_fuzzy_cursor_id(const ScFuzzy *fuzzy);

/**
 * Runs the finder in multi-select mode. Writes the checked rows' add-order
 * indices in display order; `*count_io` is in:capacity / out:count written.
 * Space toggles the cursor row. Mirrors `sc_select_run`.
 */
SPARCLI_EXPORT ScInputStatus sc_fuzzy_run_multi(
    ScFuzzy *fuzzy, size_t *indices, size_t *count_io
);

/** Pre-checks/unchecks the row at `index` (add order) for multi-select. */
SPARCLI_EXPORT void sc_fuzzy_set_checked(
    ScFuzzy *fuzzy, size_t index, bool on
);

/** Reports whether the row at `index` (add order) is checked. */
SPARCLI_EXPORT bool sc_fuzzy_is_checked(const ScFuzzy *fuzzy, size_t index);

/** Checks or unchecks every selectable row. */
SPARCLI_EXPORT void sc_fuzzy_check_all(ScFuzzy *fuzzy, bool on);

/** Returns the number of checked rows. */
SPARCLI_EXPORT size_t sc_fuzzy_checked_count(const ScFuzzy *fuzzy);

/** Pre-positions the cursor on `index` (add order, clamped to a selectable). */
SPARCLI_EXPORT void sc_fuzzy_set_cursor(ScFuzzy *fuzzy, size_t index);

/** Returns the first field of the row at `index` (add order), or `NULL`. */
SPARCLI_EXPORT const char *sc_fuzzy_label(
    const ScFuzzy *fuzzy, size_t index
);

/** Replaces the first field of the row at `index` (add order); copied. */
SPARCLI_EXPORT void sc_fuzzy_set_label(
    ScFuzzy *fuzzy, size_t index, const char *label
);

/** Replaces all fields of the row at `index` (add order); copied. Drops any
    per-cell styles/rich texts previously set on that row. */
SPARCLI_EXPORT void sc_fuzzy_set_row(
    ScFuzzy *fuzzy, size_t index, const char *const *fields, size_t n
);

/** Sets the base style of cell `col` in the row at `index` (add order). */
SPARCLI_EXPORT void sc_fuzzy_set_row_style(
    ScFuzzy *fuzzy, size_t index, size_t col, ScTextStyle style
);

/**
 * Runs the interactive finder.
 *
 * Typing edits the query; arrow keys, Tab/Shift-Tab and Ctrl-N / Ctrl-P move
 * the cursor; Enter selects the highlighted row; Esc or Ctrl-C cancels. With
 * `opts.modal` the key handling is split into normal/insert modes (see
 * `ScFuzzyOpts.modal`).
 *
 * @param fuzzy      Finder to run.
 * @param out_index  Receives the chosen item's original add-order index.
 * @return           `SC_INPUT_OK`, `SC_INPUT_CANCELLED`, or `SC_INPUT_ERROR`.
 */
SPARCLI_EXPORT ScInputStatus sc_fuzzy_run(ScFuzzy *fuzzy, size_t *out_index);

/**
 * Returns the original add-order index of the currently highlighted row.
 *
 * Intended for a `SC_SHORTCUT_CALLBACK` callback, which receives the
 * `ScFuzzy*` via its `user` pointer and can query the live selection. Returns
 * 0 when there is no current match or `fuzzy` is `NULL`.
 */
SPARCLI_EXPORT size_t sc_fuzzy_cursor_index(const ScFuzzy *fuzzy);

/**
 * Reports whether a row is currently matched (so the highlighted selection is
 * valid). Because `sc_fuzzy_cursor_index` returns `0` both for "no match" and
 * for the first row, a callback that must distinguish the two checks this
 * first. Returns `false` when the filter excludes every row or `fuzzy` is
 * `NULL`.
 *
 * @param fuzzy  Finder handle; may be `NULL`.
 * @return       `true` when at least one row matches the current query.
 */
SPARCLI_EXPORT bool sc_fuzzy_has_selection(const ScFuzzy *fuzzy);

/**
 * Removes the row at `index` (0-based, current add order), freeing its fields.
 * Intended for a `SC_SHORTCUT_CALLBACK` to delete the highlighted row live:
 * pair it with `sc_fuzzy_cursor_index`. The filtered view and cursor are
 * rebuilt. No-op when `index` is out of range. Removing the last match leaves
 * an empty result set (Enter can no longer submit; Esc cancels).
 */
SPARCLI_EXPORT void sc_fuzzy_remove(ScFuzzy *fuzzy, size_t index);

/**
 * Re-applies the current query to the rows and re-clamps the cursor, redrawing
 * the finder on the next frame. Call it after appending rows mid-run (e.g. the
 * data source grew) so the new items appear without closing the finder: bind a
 * `SC_SHORTCUT_CALLBACK` "refresh" key whose `on_fire` adds rows via
 * `sc_fuzzy_add`/`sc_fuzzy_add_row` and then calls this. No-op outside a run.
 */
SPARCLI_EXPORT void sc_fuzzy_refresh(ScFuzzy *fuzzy);

/**
 * Frees a finder and all owned rows.
 *
 * @param fuzzy  Finder to free; safe to pass `NULL`.
 */
SPARCLI_EXPORT void sc_fuzzy_free(ScFuzzy *fuzzy);

SPARCLI_END_DECLS
