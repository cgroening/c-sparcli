#pragma once

#include "sparcli.h"
#include "tty/tty_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


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
static inline bool sc_style_set(ScTextStyle style) {
    return style.attr != 0 || style.fg.index != 0 || style.bg.index != 0;
}

/** Resolves the zero-init `DEFAULT` layout to the built-in default (inline). */
static inline ScHintLayout sc_hint_resolved(ScHintLayout layout) {
    return layout == SC_HINT_LAYOUT_DEFAULT ? SC_HINT_INLINE : layout;
}

/**
 * Appends the key-hint footer to `text` according to `layout`:
 * `SC_HINT_HIDDEN` appends nothing; `SC_HINT_INLINE` appends the hint verbatim
 * on one line; `SC_HINT_STACKED` appends one line per ` · `-separated item.
 *
 * `hint` is the resolved string (widget default or caller override); a NULL or
 * empty hint appends nothing. `lead_newline` prepends a newline before the
 * footer — true when appending under existing frame content (inline widgets),
 * false when building a standalone footer (boxed/fuzzy footers).
 */
static inline void sc_append_hint(
    ScText *text, const char *hint, ScHintLayout layout, ScTextStyle style,
    bool lead_newline
) {
    layout = sc_hint_resolved(layout);
    if (layout == SC_HINT_HIDDEN || !hint || !hint[0]) {
        return;
    }
    ScTextStyle resolved = sc_style_set(style)
        ? style
        : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                         SC_ANSI_COLOR_NONE };

    if (layout == SC_HINT_INLINE) {
        if (lead_newline) {
            sc_text_append(text, "\n", (ScTextStyle){ 0 });
        }
        sc_text_append(text, hint, resolved);
        return;
    }

    /* Stacked: split the hint on " · " and emit one item per line. */
    const char *const separator = " \xc2\xb7 ";  /* space + U+00B7 + space */
    const size_t separator_len = 4;
    const char *cursor = hint;
    bool first = true;
    for (;;) {
        const char *hit = strstr(cursor, separator);
        size_t span = hit ? (size_t)(hit - cursor) : strlen(cursor);
        if (lead_newline || !first) {
            sc_text_append(text, "\n", (ScTextStyle){ 0 });
        }
        /* sc_text_append needs a NUL-terminated string; copy the one item. */
        char *item = malloc(span + 1);
        if (item) {
            memcpy(item, cursor, span);
            item[span] = '\0';
            sc_text_append(text, item, resolved);
            free(item);
        }
        first = false;
        if (!hit) {
            break;
        }
        cursor = hit + separator_len;
    }
}


/* ── Prompt loop engine (prompt.c) ──────────────────────────────────────── */

/** Per-widget behaviour passed to `sc_prompt_run`. */
typedef struct ScPromptVTable {
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
typedef struct ScLineEditor {
    /** UTF-8 bytes, always NUL-terminated. */
    char *buf;

    /** Bytes used (excluding the NUL). */
    size_t len;

    /** Allocated capacity. */
    size_t cap;

    /** Cursor byte offset into `buf`. */
    size_t cursor;
} ScLineEditor;

/** Initializes `self` with an optional initial value (copied). */
void sc_le_init(ScLineEditor *self, const char *initial);

/** Releases the buffer owned by `self`. */
void sc_le_free(ScLineEditor *self);

/**
 * Applies an editing key (insert char, backspace, delete, cursor motion,
 * word/line kill). Returns `true` when the key was an editing action the
 * editor consumed, `false` for keys the widget should handle itself
 * (Enter, Esc, arrows the widget reserves, …).
 */
bool sc_le_handle(ScLineEditor *self, ScKey key);

/**
 * Appends the editor content to `text` as styled spans, including a
 * reverse-style block at the cursor position.
 *
 * @param cursor_style       Style of the cursor cell; when unset (zero-init)
 *                           falls back to black-on-white.
 * @param mask               When non-NULL, every character is rendered as
 *                           this glyph (password masking); "" hides content.
 * @param placeholder        Shown dim when the buffer is empty; may be NULL.
 * @param field_width        Visible width in columns; 0 = unlimited. When the
 *                           content is wider, a cursor-tracking window scrolls
 *                           horizontally and shows `‹`/`›` edge markers.
 */
void sc_le_render_into(
    const ScLineEditor *self, ScText *text,
    ScTextStyle value_style, ScTextStyle cursor_style, const char *mask,
    const char *placeholder, ScTextStyle placeholder_style, int field_width
);


/* ── Shared text-entry core (text_input.c) ──────────────────────────────── */

/**
 * Configuration for the shared single-line entry core. Drives both
 * `sc_text_input` (mask == NULL) and `sc_password_input` (mask != NULL), and
 * the snapshot builder. Zero-init fields select the built-in defaults.
 */
typedef struct ScTextEntryCfg {
    const char *prompt;

    /** Prefilled value (live) / shown value (frame). */
    const char *initial;
    const char *placeholder;

    /** NULL = plain text. */
    const char *mask;
    ScTextStyle prompt_style;
    ScTextStyle value_style;

    /** Zero-init = black-on-white. */
    ScTextStyle cursor_style;

    /** Zero-init = red. */
    ScTextStyle error_style;

    /** Zero-init = dim. */
    ScTextStyle count_style;
    ScTextStyle summary_style;
    bool hide_summary;
    bool hide_char_count;

    /** 0 = unlimited. */
    int max_chars;

    /** Render inside a bordered panel. */
    bool boxed;

    /** Box border; zero-init type = rounded. */
    ScBorderStyle border;

    /** Field width; 0 = terminal width. */
    int width;

    /** Key-hint footer; NULL = default. */
    const char *hint;
    ScHintLayout hint_layout;
    ScTextStyle hint_style;
    ScCharFilter char_filter;
    void *char_filter_ctx;
    const char *const *suggestions;
    size_t n_suggestions;
    bool (*validate)(const char *, void *, const char **);
    void *validate_ctx;
} ScTextEntryCfg;

/**
 * Runs a single-line text prompt. On `SC_INPUT_OK`, `*out` receives a
 * heap-allocated copy of the value (caller frees).
 */
ScInputStatus sc_text_entry(const ScTextEntryCfg *cfg, char **out);


/* ── Snapshot frame builders (used by the non-interactive style tests) ───── */

/**
 * Each widget exposes a frame builder that runs its normal render path over
 * a constructed state and returns the captured `ScRendered`, so style tests
 * can show a widget in a given style without entering the interactive loop.
 * Print the result with `sc_pad_print(r, (ScPadOpts){0})`; free with
 * `sc_rendered_free`.
 */
ScRendered *sc_confirm_frame(const char *question, bool yes, ScConfirmOpts opts);
ScRendered *sc_text_entry_frame(const ScTextEntryCfg *cfg);
ScRendered *sc_select_frame(ScSelect *select);
ScRendered *sc_fuzzy_frame(ScFuzzy *fuzzy, const char *query);
ScRendered *sc_datepicker_frame(const struct tm *seed, ScDatePickerOpts opts);
ScRendered *sc_textarea_frame(const char *prompt, const char *content, ScTextareaOpts opts);
ScRendered *sc_number_frame(const char *prompt, double value, ScNumberOpts opts);


/* ── Theme merge (theme.c): fills zero-init opts from the global theme ───── */

void sc_theme_apply_confirm   (ScConfirmOpts    *o);
void sc_theme_apply_text      (ScTextInputOpts  *o);
void sc_theme_apply_password  (ScPasswordOpts   *o);
void sc_theme_apply_number    (ScNumberOpts     *o);
void sc_theme_apply_textarea  (ScTextareaOpts   *o);
void sc_theme_apply_select    (ScSelectOpts     *o);
void sc_theme_apply_fuzzy     (ScFuzzyOpts      *o);
void sc_theme_apply_datepicker(ScDatePickerOpts *o);
