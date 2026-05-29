#pragma once

#include "sparcli.h"
#include "tty/tty_internal.h"

#include <stdbool.h>
#include <stddef.h>


/**
 * @file input_internal.h
 * @brief Shared engine + line editor behind every input widget.
 *
 * The widgets are thin: they own a small state struct and provide two
 * callbacks (render a frame, handle a key). `sc_prompt_run` drives the raw
 * mode, the in-place redraw loop, and the cleanup. Frames are built with
 * the ordinary output renderers (`ScText` + `sc_capture_text`), so the
 * input side reuses the entire output stack as its "view" layer.
 */


/**
 * Returns `true` when a style carries any formatting. Used by every widget
 * to decide whether a caller-supplied `*_style` overrides the built-in
 * default (zero-init `ScTextStyle` = "use default"). Self-contained so the
 * input layer needs no `internal.h` dependency.
 */
static inline bool sc_style_set(ScTextStyle s) {
    return s.attr != 0 || s.fg.index != 0 || s.bg.index != 0;
}


/* ── Prompt loop engine (prompt.c) ──────────────────────────────────────── */

/**
 * Per-widget behaviour passed to `sc_prompt_run`.
 */
typedef struct {
    /** Render the current state into a fresh `ScRendered` (engine frees it). */
    ScRendered *(*render)(void *state);
    /** Handle one key; set `*done` to accept, `*cancel` to abort. */
    void (*on_key)(void *state, ScKey key, bool *done, bool *cancel);
} ScPromptVTable;

/**
 * Runs the interactive loop for one widget.
 *
 * Enters raw mode, then repeatedly renders the state, draws it in place,
 * reads a decoded key and dispatches it, until the widget signals done or
 * cancel (or EOF / Ctrl-C). Clears the interactive region and restores the
 * terminal before returning.
 *
 * @return `SC_INPUT_OK` on accept, `SC_INPUT_CANCELLED` on abort,
 *         `SC_INPUT_ERROR` when no terminal is available.
 */
ScInputStatus sc_prompt_run(const ScPromptVTable *vtable, void *state);


/* ── Shared line editor (line_editor.c) ─────────────────────────────────── */

/**
 * A growable UTF-8 edit buffer with a byte-offset cursor. Shared by the
 * text/password inputs and the fuzzy finder's query field.
 */
typedef struct {
    char  *buf;     /**< UTF-8 bytes, always NUL-terminated. */
    size_t len;     /**< Bytes used (excluding the NUL). */
    size_t cap;     /**< Allocated capacity. */
    size_t cursor;  /**< Cursor position as a byte offset into `buf`. */
} ScLineEditor;

/** Initializes `e` with an optional initial value (copied). */
void sc_le_init(ScLineEditor *e, const char *initial);

/** Releases the buffer owned by `e`. */
void sc_le_free(ScLineEditor *e);

/**
 * Applies an editing key (insert char, backspace, delete, cursor motion,
 * word/line kill). Returns `true` when the key was an editing action the
 * editor consumed, `false` for keys the widget should handle itself
 * (Enter, Esc, arrows the widget reserves, …).
 */
bool sc_le_handle(ScLineEditor *e, ScKey key);

/**
 * Appends the editor content to `text` as styled spans, including a
 * reverse-style block at the cursor position.
 *
 * @param cursor_style       Style of the cursor cell; when unset (zero-init)
 *                           falls back to black-on-white.
 * @param mask               When non-NULL, every character is rendered as
 *                           this glyph (password masking); "" hides content.
 * @param placeholder        Shown dim when the buffer is empty; may be NULL.
 */
void sc_le_render_into(
    const ScLineEditor *e, ScText *text,
    ScTextStyle value_style, ScTextStyle cursor_style, const char *mask,
    const char *placeholder, ScTextStyle placeholder_style
);


/* ── Shared text-entry core (text_input.c) ──────────────────────────────── */

/**
 * Runs a single-line text prompt. Drives both `sc_text_input` (mask == NULL)
 * and `sc_password_input` (mask != NULL).
 *
 * On `SC_INPUT_OK`, `*out` receives a heap-allocated copy of the value.
 */
ScInputStatus sc_text_entry(
    const char *prompt, char **out,
    const char *initial, const char *placeholder, const char *mask,
    ScTextStyle prompt_style, ScTextStyle value_style, ScTextStyle cursor_style,
    ScTextStyle error_style, ScTextStyle summary_style, bool hide_summary,
    bool (*validate)(const char *, void *, const char **), void *validate_ctx
);


/* ── Snapshot frame builders (used by the non-interactive style tests) ───── */

/**
 * Each widget exposes a frame builder that runs its normal render path over
 * a constructed state and returns the captured `ScRendered`, so style tests
 * can show a widget in a given style without entering the interactive loop.
 * Print the result with `sc_pad_print(r, (ScPadOpts){0})`; free with
 * `sc_rendered_free`.
 */
ScRendered *sc_confirm_frame(const char *question, bool yes, ScConfirmOpts opts);
ScRendered *sc_text_entry_frame(
    const char *prompt, const char *value, const char *placeholder,
    const char *mask, ScTextStyle prompt_style, ScTextStyle value_style,
    ScTextStyle cursor_style
);
ScRendered *sc_select_frame(ScSelect *select);
void        sc_select_check(ScSelect *select, size_t index, bool on);
ScRendered *sc_fuzzy_frame(ScFuzzy *fuzzy, const char *query);
ScRendered *sc_datepicker_frame(const struct tm *seed, ScDatePickerOpts opts);
