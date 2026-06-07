#pragma once

#include "core/sparcli_core.h"
#include "input/sparcli_term.h"
#include "input/sparcli_shortcut.h"

#include <time.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_form.h
 * @brief Grid-layout form: framed fields arranged in a row/column raster,
 *        navigated in 2D and edited one at a time in a region below the grid.
 *
 * A form is a set of fields laid out like a table (rows of cells, with
 * `col_span`/`row_span` and skip placeholders for spanned cells). Each field
 * renders as a bordered box showing its label (border title) and current
 * value. Arrow keys move the highlighted box in two dimensions; Enter opens an
 * editor for the active field BELOW the grid; a second Enter saves and the box
 * is re-rendered with the new value; Esc leaves the editor (Esc in navigation
 * cancels the whole form); Ctrl-D submits.
 *
 * Like every input widget the form is a single interactive session - it must
 * not be run nested inside another prompt. Header is part of the
 * `input/sparcli_input.h` sub-umbrella (so reachable via `<sparcli.h>`).
 */

/** Field editor kind. */
typedef enum ScFieldType {
    SC_FIELD_TEXT = 0,
    SC_FIELD_NUMBER,
    SC_FIELD_BOOL,
    SC_FIELD_SELECT,       /**< Single choice from a list. */
    SC_FIELD_MULTISELECT,  /**< Multiple choices (checkboxes). */
    SC_FIELD_DATE,         /**< Calendar date (month-grid picker). */
} ScFieldType;

/** How a field's grid-column width is sized. */
typedef enum ScFieldWidthMode {
    SC_FWIDTH_AUTO = 0, /**< Share the leftover width equally. */
    SC_FWIDTH_PCT,      /**< `width` percent of the available row width. */
    SC_FWIDTH_FIXED,    /**< `width` columns. */
} ScFieldWidthMode;

/**
 * Validation callback. Return `true` to accept `value`; on `false`, set
 * `*err` to a short message (borrowed, shown in red below the editor) and the
 * editor stays open. Used by text/number fields.
 */
typedef bool (*ScFieldValidate)(const char *value, void *ctx, const char **err);

/**
 * Per-field layout and behaviour. Zero-init selects sensible defaults: auto
 * width, span 1x1, one content line, optional (not required), rounded border.
 *
 * Grid sizing: a field's column width is driven by single-column fields
 * (`col_span == 1`) anchored in that column - the last one wins. A spanning
 * field (`col_span > 1`) just sums the columns it covers; a spanning field
 * (`row_span > 1`) fills the height of the rows it covers.
 */
typedef struct ScFieldOpts {
    /** Column width mode; zero-init = AUTO. */
    ScFieldWidthMode width_mode;

    /** Percent (PCT) or column count (FIXED); ignored for AUTO. */
    int width;

    /** Columns spanned; zero-init = 1. */
    int col_span;

    /** Rows spanned; zero-init = 1. */
    int row_span;

    /** Content lines inside the box; zero-init = 1. */
    int height;

    /** Block form submit until non-empty / changed. */
    bool required;

    /**
     * Text field only: the value may contain newlines. The box shows it across
     * its content lines and the field is edited via the external editor
     * (`ScFormOpts.editor_key`, default Ctrl-G) instead of an inline editor.
     */
    bool multiline;

    /**
     * Date field only: the date may be absent ("no date"). The field then starts
     * empty when its `initial` is a zeroed `struct tm` (showing the placeholder
     * instead of today), Delete/Backspace clears it back to empty while editing,
     * and `sc_form_get_date` returns `false` for an empty field. Zero-init = off
     * (a date field always holds a date; a zeroed `initial` means today).
     */
    bool date_optional;

    /** One-line help shown in the editor region; may be NULL. */
    const char *help;

    /** Box border; zero-init type = rounded. */
    ScBorderStyle border;

    /** Optional validation (text/number); NULL = none. */
    ScFieldValidate validate;
    void *validate_ctx;
} ScFieldOpts;

/** Options for the whole form. Zero-init friendly. */
typedef struct ScFormOpts {
    /** Heading shown above the grid; may be NULL. */
    const char *title;

    /** Style of the title; zero-init = bold. */
    ScTextStyle title_style;

    /** Highlight color of the active box / editor; zero-init = cyan. */
    ScColor accent;

    /** Key-hint footer override; NULL = sensible default. */
    const char *hint;

    /** Key-hint footer layout / placement / style. */
    ScHintLayout hint_layout;
    ScHintPosition hint_pos;
    ScTextStyle hint_style;

    /** Style of the persistent summary line. */
    ScTextStyle summary_style;

    /** Suppress the post-submit summary line. */
    bool hide_summary;

    /**
     * Disable arrow-key cycling. By default the active box wraps around the grid
     * edges (Down past the last row returns to the top in the same column, Right
     * past the last column returns to the first in the same row, etc.); set this
     * to stop at the edges instead. Tab/Shift-Tab always cycle.
     */
    bool no_cycle;

    /** Custom key shortcuts (borrowed, must outlive the run). */
    const ScShortcut *shortcuts;
    size_t n_shortcuts;
    int *out_shortcut_id;

    /**
     * External editor for `multiline` fields. `editor` is the command
     * (NULL = `$VISUAL`/`$EDITOR`/nvim/vi); `editor_key` opens it on the active
     * multiline field (zero-init = Ctrl-G). No effect without a multiline field.
     */
    const char *editor;
    ScKeyChord editor_key;

    /** Background of the editor box shown below the grid while editing a field.
        Zero-init = a subtle gray default; set a named/RGB color
        (`sc_color_from_rgb(...)`) to customize the tone. */
    ScColor edit_bg;

    /** Full-screen mode: compose `[valign-pad][header][grid]` filling the
        terminal height (for a consistent shell alongside a fullscreen finder).
        Run inside an `sc_altscreen_begin` session. The grid is fixed-size (no
        auto-grow); only the alignment/header are added. */
    bool fullscreen;

    /** Vertical alignment of the (header + grid) block (fullscreen only). */
    ScVAlign valign;

    /** Optional header pinned above the grid (fullscreen only); borrowed,
        library-rendered. Must outlive the run. */
    const struct ScRendered *header;
} ScFormOpts;

/** Opaque form handle. */
typedef struct ScForm ScForm;

/** Creates a form. Opts strings are copied. Returns NULL on OOM. */
SPARCLI_EXPORT ScForm *sc_form_new(ScFormOpts opts);

/** Starts a new grid row. Call before adding the row's fields. */
SPARCLI_EXPORT void sc_form_row_begin(ScForm *form);

/**
 * Adds a field to the current row. Returns the field index (>= 0) used by the
 * getters, or -1 on error. `label` and `initial` are copied.
 */
SPARCLI_EXPORT int sc_form_add_text(
    ScForm *form, const char *label, const char *initial, ScFieldOpts opts
);
SPARCLI_EXPORT int sc_form_add_number(
    ScForm *form, const char *label, double initial, ScFieldOpts opts
);
SPARCLI_EXPORT int sc_form_add_bool(
    ScForm *form, const char *label, bool initial, ScFieldOpts opts
);

/**
 * Single-choice field. `choices` (and each string) are copied; `initial` is the
 * preselected index (clamped to range). The editor opens a scrollable list
 * below the grid; Enter selects.
 */
SPARCLI_EXPORT int sc_form_add_select(
    ScForm *form, const char *label, const char *const *choices, size_t n,
    size_t initial, ScFieldOpts opts
);

/**
 * Multi-choice field. `choices` are copied; `checked_indices` (length
 * `n_checked`, may be NULL) preselects entries. The editor opens a checkbox
 * list below the grid; Space toggles, Enter confirms.
 */
SPARCLI_EXPORT int sc_form_add_multiselect(
    ScForm *form, const char *label, const char *const *choices, size_t n,
    const size_t *checked_indices, size_t n_checked, ScFieldOpts opts
);

/**
 * Date field. `initial` seeds the picker (a zeroed `struct tm` starts at
 * today). The editor opens a month grid below the grid: arrows move by
 * day/week, PageUp/Down or `<`/`>` change month, Shift+PageUp/Down change year,
 * Enter picks. Read back with `sc_form_get_date`.
 */
SPARCLI_EXPORT int sc_form_add_date(
    ScForm *form, const char *label, struct tm initial, ScFieldOpts opts
);

/** Adds a skip placeholder covering a cell of a col/row-spanning field. */
SPARCLI_EXPORT void sc_form_add_skip(ScForm *form);

/**
 * Runs the form interactively. On `SC_INPUT_OK` the edited values are readable
 * with the getters below. Returns `SC_INPUT_CANCELLED` on Esc/Ctrl-C and
 * `SC_INPUT_ERROR` when no terminal is available.
 */
SPARCLI_EXPORT ScInputStatus sc_form_run(ScForm *form);

/** Current text of a field (borrowed; valid until the form is freed/edited). */
SPARCLI_EXPORT const char *sc_form_get_string(const ScForm *form, int field);

/** Numeric value of a number field (0 for non-number / invalid index). */
SPARCLI_EXPORT double sc_form_get_number(const ScForm *form, int field);

/** Boolean value of a bool field (false for non-bool / invalid index). */
SPARCLI_EXPORT bool sc_form_get_bool(const ScForm *form, int field);

/** Selected index of a single-select field (0 for non-select/invalid). */
SPARCLI_EXPORT size_t sc_form_get_choice(const ScForm *form, int field);

/**
 * Checked indices of a multiselect field, written (add-order) into `out` up to
 * `cap`. Returns the total checked count (may exceed `cap`). `out` may be NULL
 * to query the count.
 */
SPARCLI_EXPORT size_t sc_form_get_checked(
    const ScForm *form, int field, size_t *out, size_t cap
);

/**
 * Picked date of a date field, written to `*out` (normalized via mktime).
 * Returns false for a non-date field or invalid index.
 */
SPARCLI_EXPORT bool sc_form_get_date(
    const ScForm *form, int field, struct tm *out
);

/** Frees the form and all owned strings. NULL-safe. */
SPARCLI_EXPORT void sc_form_free(ScForm *form);

SPARCLI_END_DECLS
