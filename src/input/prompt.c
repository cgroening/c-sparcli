#include "input_internal.h"

#include <stdatomic.h>


/*
 * Guards against concurrent / nested prompts: a single process-wide terminal
 * session exists (one keyboard, one screen, process-global signal handlers), so
 * only one prompt may run at a time. The atomic claim makes a misuse from
 * another thread fail safely (SC_INPUT_ERROR) instead of corrupting state.
 */
static atomic_bool g_prompt_active = false;


/**
 * Renders exactly `n` blank lines, or NULL when n <= 0. `sc_capture_str` yields
 * one line per `\n`, so a string of `n` newlines captures to `n` empty lines
 * (an empty string captures to zero lines - hence the explicit count).
 */
static ScRendered *blank_lines(int n) {
    if (n <= 0) {
        return NULL;
    }
    char *buf = calloc((size_t)n + 1, 1);
    if (buf) {
        for (int i = 0; i < n; i++) { buf[i] = '\n'; }
    }
    ScRendered *pad = sc_capture_str(buf ? buf : "");
    free(buf);
    return pad;
}

ScRendered *sc_fullscreen_compose(ScRendered *body, const ScRendered *header,
                                  const ScRendered *footer, ScVAlign valign,
                                  ScVAlignScope scope, int bottom_reserve) {
    if (!body) {
        return body;
    }
    int rows = sc_term_height();
    if (bottom_reserve > 0) { rows -= bottom_reserve; }   /* leave room below */
    int header_h = header ? (int)header->line_count : 0;
    int footer_h = footer ? (int)footer->line_count : 0;

    if (scope == SC_VALIGN_SCOPE_CONTENT) {
        /* Header pinned to the top, footer pinned to the bottom; only the body
           is aligned in the gap between them:
           [header][top pad][body][bottom pad][footer]. */
        int free_rows = rows - header_h - footer_h - (int)body->line_count;
        if (free_rows < 0) { free_rows = 0; }
        int top_pad = valign == SC_VALIGN_MIDDLE ? free_rows / 2
                    : valign == SC_VALIGN_BOTTOM ? free_rows : 0;
        int bot_pad = free_rows - top_pad;

        const ScRendered *parts[5];
        size_t n = 0;
        ScRendered *top = blank_lines(top_pad);
        ScRendered *bot = blank_lines(bot_pad);
        if (header) { parts[n++] = header; }
        if (top)    { parts[n++] = top; }
        parts[n++] = body;
        if (bot)    { parts[n++] = bot; }
        if (footer) { parts[n++] = footer; }

        ScRendered *out = n > 1 ? sc_vstack(parts, n, 0) : NULL;
        sc_rendered_free(top);
        sc_rendered_free(bot);
        if (out) {
            sc_rendered_free(body);
            return out;
        }
        return body;   // OOM / nothing to add: leave the body as-is
    }

    /* SC_VALIGN_SCOPE_ALL: align the [header][body][footer] block as one unit. */
    int free_rows =
        rows - header_h - footer_h - (int)body->line_count;
    if (free_rows < 0) { free_rows = 0; }
    int top_pad = valign == SC_VALIGN_MIDDLE ? free_rows / 2
                : valign == SC_VALIGN_BOTTOM ? free_rows : 0;

    // block = [header][body][footer], collapsing absent parts.
    ScRendered *block = body;
    bool block_is_new = false;
    if (header || footer) {
        const ScRendered *parts[3];
        size_t n = 0;
        if (header) { parts[n++] = header; }
        parts[n++] = body;
        if (footer) { parts[n++] = footer; }
        ScRendered *joined = sc_vstack(parts, n, 0);
        if (joined) { block = joined; block_is_new = true; }
    }
    if (top_pad <= 0) {
        if (block_is_new) { sc_rendered_free(body); }
        return block;
    }

    ScRendered *pad = blank_lines(top_pad);
    if (!pad) {                          // OOM: skip the padding
        if (block_is_new) { sc_rendered_free(body); }
        return block;
    }
    const ScRendered *parts[2] = { pad, block };
    ScRendered *out = sc_vstack(parts, 2, 0);
    sc_rendered_free(pad);
    if (!out) {                          // OOM: fall back to the block
        if (block_is_new) { sc_rendered_free(body); }
        return block;
    }
    if (block_is_new) { sc_rendered_free(block); }
    sc_rendered_free(body);
    return out;
}


/**
 * Builds a dim one-line footer listing the labeled shortcuts (e.g.
 * `^X delete  ·  F2 help`), or NULL when none carry a `hint_label`. The engine
 * stacks it under every frame so custom shortcuts are discoverable on any
 * widget without per-widget code.
 */
static ScRendered *build_shortcut_hint(const ScPromptShortcuts *shortcuts) {
    if (!shortcuts || shortcuts->count == 0) {
        return NULL;
    }
    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }
    const ScTextStyle dim = {
        SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
    };
    bool any = false;
    for (size_t i = 0; i < shortcuts->count; i++) {
        const ScShortcut *s = &shortcuts->items[i];
        if (!s->hint_label || !s->hint_label[0] || s->hide_in_footer) {
            continue;
        }
        char key[16];
        sc_key_chord_name(s->chord, key, sizeof key);
        if (any) {
            sc_text_append(text, "  \xc2\xb7  ", dim);   /* "  ·  " */
        }
        sc_text_append(text, key, dim);
        sc_text_append(text, " ", dim);
        sc_text_append(text, s->hint_label, dim);
        any = true;
    }
    if (!any) {
        sc_text_free(text);
        return NULL;
    }
    ScRendered *rendered = sc_capture_text(text);
    sc_text_free(text);
    return rendered;
}

/**
 * Suspends the prompt, runs the external editor on the current text, and
 * applies the result. Restores raw mode and forces a fresh repaint. Returns
 * false only when the terminal could not be re-entered (caller should abort).
 */
static bool run_editor_action(
    const ScPromptVTable *vtable, void *state,
    const ScPromptEditor *editor, ScScreen *screen
) {
    char *current = vtable->edit_get(state);
    if (!current) {
        // The widget has nothing editable right now (e.g. a form whose active
        // field is not an editor-backed field): treat the key as a no-op.
        return true;
    }
    sc_screen_clear(screen);          // erase our region before handing over
    sc_tty_end();   // suspend: restore terminal, drop handlers

    char *edited = NULL;
    bool ok = sc_run_editor(editor->cmd, current ? current : "",
                            editor->suffix, &edited);
    free(current);

    ScInputStatus resumed = sc_tty_begin();   // resume raw mode
    sc_tty_input_reset();
    *screen = (ScScreen){ 0 };         // next draw repaints from scratch
    if (ok) {
        vtable->edit_set(state, edited);
    }
    free(edited);
    return resumed == SC_INPUT_OK;
}

/**
 * Renders one frame, stacks the labeled-shortcut footer under it, and draws it.
 * Returns false only when the widget's `render` fails (caller aborts).
 */
static bool draw_prompt_frame(
    ScScreen *screen, const ScPromptVTable *vtable, void *state,
    const ScRendered *shortcut_hint
) {
    ScRendered *frame = vtable->render(state);
    if (!frame) {
        return false;
    }
    ScRendered *combined = NULL;
    if (shortcut_hint) {
        const ScRendered *parts[2] = { frame, shortcut_hint };
        combined = sc_vstack(parts, 2, 0);
    }
    const ScRendered *draw = combined ? combined : frame;
    sc_screen_draw(screen, draw->lines, draw->line_count);
    sc_rendered_free(combined);
    sc_rendered_free(frame);
    return true;
}

/**
 * Returns true when `key` is the (opt-in) external-editor chord and the widget
 * supports editing. A zero-init chord resolves to Ctrl-G.
 */
static bool editor_key_matches(
    const ScPromptEditor *editor, const ScPromptVTable *vtable, void *state,
    ScKey key
) {
    if (!editor || !editor->enabled
        || !vtable->edit_get || !vtable->edit_set) {
        return false;
    }
    if (vtable->wants_editor && vtable->wants_editor(state, key)) {
        return true;   // widget-bound extra key (e.g. Enter on a multiline)
    }
    ScKeyChord chord = (editor->chord.key == SC_KEY_NONE
                        && editor->chord.codepoint == 0)
        ? sc_key_ctrl('g') : editor->chord;
    return sc_key_chord_matches(key, chord);
}

/**
 * Matches custom shortcuts before the widget's own keys. Returns true when a
 * shortcut fired (the caller skips `on_key`); sets `*done` when the prompt
 * should end. When `allow_char_chords` is false, a matched shortcut bound to a
 * bare printable character (no modifiers) is ignored so the key reaches the
 * widget as text (modal insert mode).
 */
static bool dispatch_shortcut(
    ScPromptShortcuts *shortcuts, ScKey key, bool *done, bool allow_char_chords
) {
    if (!shortcuts || shortcuts->count == 0) {
        return false;
    }
    const ScShortcut *hit =
        sc_shortcut_find(key, shortcuts->items, shortcuts->count);
    if (!hit) {
        return false;
    }
    if (!allow_char_chords && hit->chord.mods == 0
        && (hit->chord.key == SC_KEY_CHAR
            || hit->chord.key == SC_KEY_BACKSPACE
            || hit->chord.key == SC_KEY_DELETE)) {
        // Insert mode: bare letters and the editing keys reach the query
        // editor instead of firing a shortcut.
        return false;
    }
    if (hit->mode == SC_SHORTCUT_CALLBACK) {
        bool keep_open =
            hit->on_fire ? hit->on_fire(hit->id, hit->user) : false;
        if (!keep_open) { *done = true; }
    } else {
        if (shortcuts->out_id) { *shortcuts->out_id = hit->id; }
        *done = true;
    }
    return true;
}

/** Drives one widget's raw-mode draw/read/dispatch loop to completion. */
ScInputStatus sc_prompt_run(
    const ScPromptVTable *vtable, void *state, ScPromptShortcuts *shortcuts,
    const ScPromptEditor *editor
) {
    if (!vtable || !vtable->render || !vtable->on_key) {
        return SC_INPUT_ERROR;
    }
    // Atomically claim the single session; bail if one is already running.
    if (atomic_exchange(&g_prompt_active, true)) {
        return SC_INPUT_ERROR;
    }
    if (!sc_input_available() || sc_tty_begin() != SC_INPUT_OK) {
        atomic_store(&g_prompt_active, false);
        return SC_INPUT_ERROR;
    }

    if (shortcuts && shortcuts->out_id) {
        *shortcuts->out_id = -1;   // -1 = normal submit / cancel
    }
    // Built once: the labeled-shortcut footer is constant for the whole run.
    // Indent it (also once) to line up under the widget's framed content.
    ScRendered *shortcut_hint = build_shortcut_hint(shortcuts);
    if (shortcut_hint && vtable->hint_indent) {
        shortcut_hint = sc_indent_rendered(shortcut_hint,
                                           vtable->hint_indent(state));
    }

    ScScreen screen = { 0 };
    bool done = false;
    bool cancel = false;
    ScInputStatus status = SC_INPUT_OK;

    while (!done && !cancel) {
        if (!draw_prompt_frame(&screen, vtable, state, shortcut_hint)) {
            status = SC_INPUT_ERROR;
            break;
        }

        ScKey key = sc_tty_read_key();
        switch (key.type) {
            case SC_KEY_NONE:        // EOF / read error
            case SC_KEY_CTRL_C:
                cancel = true;
                continue;
            case SC_KEY_ESC:
                // Esc cancels unless the widget opts to handle it itself
                // (e.g. leave a modal insert mode); then it falls through to
                // the normal dispatch chain and reaches on_key.
                if (!vtable->capture_escape) {
                    cancel = true;
                    continue;
                }
                break;
            case SC_KEY_RESIZE:
                // Terminal reflowed: drop the stale region and repaint fresh.
                sc_screen_clear(&screen);
                continue;
            default:
                break;
        }

        // Pasted keys are literal text: they never match shortcuts or the
        // editor key, and pasted Enter only reaches widgets that accept
        // multi-line pastes. Widgets without a text field ignore pastes
        // entirely (SC_PASTE_IGNORE), so pasted characters cannot navigate
        // lists or answer confirmations.
        if (key.mods & SC_MOD_PASTED) {
            bool drop_key = vtable->paste == SC_PASTE_IGNORE
                || (key.type == SC_KEY_ENTER
                    && vtable->paste != SC_PASTE_MULTILINE);
            if (!drop_key) {
                vtable->on_key(state, key, &done, &cancel);
            }
            continue;
        }

        // External-editor key (opt-in; text_input/textarea only). Matched
        // before shortcuts/on_key, after the reserved cancel keys.
        if (editor_key_matches(editor, vtable, state, key)) {
            if (!run_editor_action(vtable, state, editor, &screen)) {
                status = SC_INPUT_ERROR;
                break;
            }
            continue;
        }

        // Custom shortcuts before the widget's own keys (an explicit binding
        // shadows a built-in use of that key); cancel keys above always win.
        // In modal insert mode bare-letter shortcuts are suppressed so they
        // type into the field instead of firing.
        bool allow_char_chords = !(vtable->suppress_char_shortcuts
                                   && vtable->suppress_char_shortcuts(state));
        if (dispatch_shortcut(shortcuts, key, &done, allow_char_chords)) {
            continue;
        }

        vtable->on_key(state, key, &done, &cancel);
    }

    sc_screen_clear(&screen);
    sc_tty_end();
    atomic_store(&g_prompt_active, false);
    sc_rendered_free(shortcut_hint);

    if (cancel) {
        return SC_INPUT_CANCELLED;
    }
    return status;
}
