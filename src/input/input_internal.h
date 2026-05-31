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

/** Resolves the zero-init `DEFAULT` position to the built-in default (bottom). */
static inline ScHintPosition sc_hint_pos_resolved(ScHintPosition pos) {
    return pos == SC_HINT_POS_DEFAULT ? SC_HINT_POS_BOTTOM : pos;
}


/* ── Rich prompt resolver ────────────────────────────────────────────────── */
/*
 * A widget prompt can be supplied three ways, in precedence order:
 *   1. `rich`  – a caller-built ScText (mixed styles; no escaping needed),
 *   2. `markup`– parse the plain string as inline markup ("[italic]x[/]"),
 *   3. plain   – the string rendered with a single `style`.
 * These helpers centralize that resolution so every widget (inline and boxed)
 * behaves identically. Public API only, so this header stays internal.h-free.
 */

/** Appends the resolved prompt spans into `dst`. */
static inline void sc_prompt_append(
    ScText *dst, const char *plain, ScTextStyle style,
    bool markup, const ScText *rich
) {
    if (rich) {
        size_t n = sc_text_span_count(rich);
        for (size_t i = 0; i < n; i++) {
            ScSpan span = sc_text_span(rich, i);
            if (span.raw_str) { sc_text_append(dst, span.raw_str, span.style); }
        }
    } else if (markup && plain) {
        sc_markup_append(dst, plain);
    } else if (plain) {
        sc_text_append(dst, plain, style);
    }
}

/** Builds a standalone resolved prompt `ScText` (caller frees), or NULL. */
static inline ScText *sc_prompt_build(
    const char *plain, ScTextStyle style, bool markup, const ScText *rich
) {
    ScText *text = sc_text_new();
    if (!text) { return NULL; }
    sc_prompt_append(text, plain, style, markup, rich);
    return text;
}

/** Visible column width of the resolved prompt. */
static inline int sc_prompt_width(
    const char *plain, ScTextStyle style, bool markup, const ScText *rich
) {
    if (rich) { return (int)sc_text_visible_width(rich); }
    if (!markup) {
        /* Fast path: a plain prompt's width is its UTF-8 codepoint count
         * (count non-continuation bytes), no allocation on the render path. */
        int width = 0;
        for (const unsigned char *p = (const unsigned char *)plain;
             plain && *p; p++) {
            if ((*p & 0xC0) != 0x80) { width++; }
        }
        return width;
    }
    ScText *text = sc_prompt_build(plain, style, markup, rich);
    int width = text ? (int)sc_text_visible_width(text) : 0;
    sc_text_free(text);
    return width;
}

/**
 * Returns a heap-allocated plain-text (style-stripped) version of the resolved
 * prompt, for one-line summaries. Caller frees. Never returns NULL for valid
 * input (empty string on OOM-free trivial paths).
 */
static inline char *sc_prompt_plain(
    const char *plain, ScTextStyle style, bool markup, const ScText *rich
) {
    if (!markup && !rich) {
        return strdup(plain ? plain : "");
    }
    ScText *text = sc_prompt_build(plain, style, markup, rich);
    size_t n = text ? sc_text_span_count(text) : 0;
    size_t total = 1;
    for (size_t i = 0; i < n; i++) {
        ScSpan span = sc_text_span(text, i);
        if (span.raw_str) { total += strlen(span.raw_str); }
    }
    char *out = malloc(total);
    if (out) {
        size_t pos = 0;
        for (size_t i = 0; i < n; i++) {
            ScSpan span = sc_text_span(text, i);
            if (span.raw_str) {
                size_t len = strlen(span.raw_str);
                memcpy(out + pos, span.raw_str, len);
                pos += len;
            }
        }
        out[pos] = '\0';
    }
    sc_text_free(text);
    return out;
}

/**
 * Builds the key-hint footer as a standalone `ScRendered` per `layout`:
 * `SC_HINT_INLINE` is one line, `SC_HINT_STACKED` is one line per ` · `-
 * separated item. Returns NULL when hidden, empty, or on allocation failure.
 * Independent of placement — `sc_hint_place` positions the result.
 *
 * `hint` is the resolved string (widget default or caller override).
 */
static inline ScRendered *sc_hint_render(
    const char *hint, ScHintLayout layout, ScTextStyle style
) {
    layout = sc_hint_resolved(layout);
    if (layout == SC_HINT_HIDDEN || !hint || !hint[0]) {
        return NULL;
    }
    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }
    ScTextStyle resolved = sc_style_set(style)
        ? style
        : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                         SC_ANSI_COLOR_NONE };

    if (layout == SC_HINT_INLINE) {
        sc_text_append(text, hint, resolved);
    } else {
        /* Stacked: split the hint on " · " and emit one item per line. */
        const char *const separator = " \xc2\xb7 ";  /* space U+00B7 space */
        const size_t separator_len = 4;
        const char *cursor = hint;
        bool first = true;
        for (;;) {
            const char *hit = strstr(cursor, separator);
            size_t span = hit ? (size_t)(hit - cursor) : strlen(cursor);
            if (!first) {
                sc_text_append(text, "\n", (ScTextStyle){ 0 });
            }
            /* sc_text_append needs a NUL-terminated string; copy the item. */
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

    ScRendered *rendered = sc_capture_text(text);
    sc_text_free(text);
    return rendered;
}

/**
 * Places the hint block around the widget body per `pos` and returns the
 * composed frame. Top/bottom stack vertically; left/right sit beside the body,
 * top-aligned, with a small gap. Both `body` and `hint` are consumed (freed).
 * A NULL `hint` returns `body` unchanged.
 */
static inline ScRendered *sc_hint_place(
    ScRendered *body, ScRendered *hint, ScHintPosition pos
) {
    if (!hint) {
        return body;
    }
    pos = sc_hint_pos_resolved(pos);

    ScRendered *out = NULL;
    if (pos == SC_HINT_POS_TOP || pos == SC_HINT_POS_BOTTOM) {
        const ScRendered *parts[2];
        parts[0] = (pos == SC_HINT_POS_TOP) ? hint : body;
        parts[1] = (pos == SC_HINT_POS_TOP) ? body : hint;
        out = sc_vstack(parts, 2, 0);
    } else {
        ScColumns *cols = sc_columns_new((ScColumnsOpts){
            .gap = 2, .valign = SC_VALIGN_TOP,
            .sep = { .type = SC_BORDER_NONE, .color = SC_ANSI_COLOR_NONE } });
        if (cols) {
            if (pos == SC_HINT_POS_LEFT) {
                sc_columns_add_rendered(cols, hint, (ScColItem){ 0 });
                sc_columns_add_rendered(cols, body, (ScColItem){ 0 });
            } else {
                sc_columns_add_rendered(cols, body, (ScColItem){ 0 });
                sc_columns_add_rendered(cols, hint, (ScColItem){ 0 });
            }
            out = sc_capture_columns(cols);
            sc_columns_free(cols);
        }
    }
    sc_rendered_free(body);
    sc_rendered_free(hint);
    return out;
}

/**
 * Composes a widget's `body` frame with its key-hint footer: builds the hint
 * per `layout`, then places it per `pos`. Consumes `body`; returns the final
 * frame. The single call every widget makes after assembling its body.
 */
static inline ScRendered *sc_compose_hint(
    ScRendered *body, const char *hint, ScHintLayout layout,
    ScHintPosition pos, ScTextStyle style
) {
    return sc_hint_place(body, sc_hint_render(hint, layout, style), pos);
}


/* ── Prompt loop engine (prompt.c) ──────────────────────────────────────── */

/** Per-widget behaviour passed to `sc_prompt_run`. */
typedef struct ScPromptVTable {
    /** Render the current state into a fresh `ScRendered` (engine frees it). */
    ScRendered *(*render)(void *state);

    /** Handle one key; set `*done` to accept, `*cancel` to abort. */
    void (*on_key)(void *state, ScKey key, bool *done, bool *cancel);

    /**
     * Optional external-editor hooks (text_input / textarea only). `edit_get`
     * returns a heap copy of the current text (caller frees); `edit_set`
     * replaces the widget's text with the editor result. Both NULL = the
     * widget does not support editing in an external editor.
     */
    char *(*edit_get)(void *state);
    void  (*edit_set)(void *state, const char *text);
} ScPromptVTable;

/**
 * Optional external-editor binding consulted by the engine. Pass `NULL` to
 * `sc_prompt_run` when no editor is configured. Active only when `enabled` is
 * true and the vtable provides `edit_get`/`edit_set`.
 */
typedef struct ScPromptEditor {
    /** Enable the editor key (opt-in). */
    bool enabled;

    /** Editor command override; `NULL`/empty = $VISUAL → $EDITOR → nvim → vi. */
    const char *cmd;

    /** Key that opens the editor; zero-init = Ctrl-G. */
    ScKeyChord chord;
} ScPromptEditor;

/**
 * Runs `cmd` (resolved per the chain above) on a temp file seeded with
 * `initial`, on the controlling terminal. On a clean exit (status 0) the saved
 * contents are returned in `*out` (heap; caller frees) and the function returns
 * `true`; on a non-zero exit, signal, or any error it returns `false` and
 * `*out` is untouched. Shell-free (`execvp` with a whitespace-tokenized argv);
 * the temp file is mode 0600 and unlinked before returning. Does not touch
 * raw mode — the engine suspends/resumes the terminal around the call.
 */
bool sc_run_editor(const char *cmd, const char *initial, char **out);

/**
 * Optional custom shortcuts consulted by the engine before `on_key`, shared
 * by every widget. Built from the widget's `Sc*Opts.shortcuts` fields. Pass
 * `NULL` to `sc_prompt_run` when no shortcuts are configured.
 */
typedef struct ScPromptShortcuts {
    /** Borrowed shortcut array (must outlive the run). */
    const ScShortcut *items;

    /** Number of entries in `items`. */
    size_t count;

    /**
     * Optional out-param: set to `-1` before the loop, then to the fired
     * shortcut's `id` when a `SC_SHORTCUT_RETURN` shortcut ends the prompt.
     * Stays `-1` on a normal submit or cancel.
     */
    int *out_id;
} ScPromptShortcuts;

/**
 * Runs the interactive loop for one widget.
 *
 * Enters raw mode, then repeatedly renders the state, draws it in place,
 * reads a decoded key and dispatches it, until the widget signals done or
 * cancel (or EOF / Ctrl-C). Each key is checked against `shortcuts` (when
 * non-NULL) before `on_key`: a RETURN shortcut ends the loop and records its
 * id; a CALLBACK shortcut runs in place and keeps the loop running unless its
 * callback returns `false`. Esc / Ctrl-C stay reserved for cancel. Clears the
 * interactive region and restores the terminal before returning.
 *
 * @return `SC_INPUT_OK` on accept, `SC_INPUT_CANCELLED` on abort,
 *         `SC_INPUT_ERROR` when no terminal is available.
 */
ScInputStatus sc_prompt_run(
    const ScPromptVTable *vtable, void *state, ScPromptShortcuts *shortcuts,
    const ScPromptEditor *editor
);


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
    ScHintPosition hint_pos;
    ScTextStyle hint_style;
    ScCharFilter char_filter;
    void *char_filter_ctx;
    const char *const *suggestions;
    size_t n_suggestions;
    bool (*validate)(const char *, void *, const char **);
    void *validate_ctx;

    /** Custom key shortcuts (borrowed) + optional fired-id out-param. */
    const ScShortcut *shortcuts;
    size_t n_shortcuts;
    int *out_shortcut_id;

    /** Rich prompt (overrides string prompt) + markup flag. */
    const ScText *prompt_text;
    bool prompt_markup;

    /** External-editor binding (text_input only; password leaves these off). */
    bool external_editor;
    const char *editor;
    ScKeyChord editor_key;
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
