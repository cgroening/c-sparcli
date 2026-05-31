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
        if (!s->hint_label || !s->hint_label[0]) {
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

/** Drives one widget's raw-mode draw/read/dispatch loop to completion. */
ScInputStatus sc_prompt_run(
    const ScPromptVTable *vtable, void *state, ScPromptShortcuts *shortcuts
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
    ScRendered *shortcut_hint = build_shortcut_hint(shortcuts);

    ScScreen screen = { 0 };
    bool done = false;
    bool cancel = false;
    ScInputStatus status = SC_INPUT_OK;

    while (!done && !cancel) {
        ScRendered *frame = vtable->render(state);
        if (!frame) {
            status = SC_INPUT_ERROR;
            break;
        }
        if (shortcut_hint) {
            // Stack the shortcut footer under the widget frame for this draw.
            const ScRendered *parts[2] = { frame, shortcut_hint };
            ScRendered *combined = sc_vstack(parts, 2, 0);
            if (combined) {
                sc_screen_draw(&screen, combined->lines, combined->line_count);
                sc_rendered_free(combined);
            } else {
                sc_screen_draw(&screen, frame->lines, frame->line_count);
            }
        } else {
            sc_screen_draw(&screen, frame->lines, frame->line_count);
        }
        sc_rendered_free(frame);

        ScKey key = sc_tty_read_key();
        switch (key.type) {
            case SC_KEY_NONE:        // EOF / read error
            case SC_KEY_CTRL_C:
            case SC_KEY_ESC:
                cancel = true;
                continue;
            case SC_KEY_RESIZE:
                // Terminal reflowed: drop the stale region and repaint fresh.
                sc_screen_clear(&screen);
                continue;
            default:
                break;
        }

        // Custom shortcuts are matched before the widget's own keys (so an
        // explicit binding shadows a built-in use of that key). Reserved
        // cancel keys above always win.
        if (shortcuts && shortcuts->count > 0) {
            const ScShortcut *hit =
                sc_shortcut_find(key, shortcuts->items, shortcuts->count);
            if (hit) {
                if (hit->mode == SC_SHORTCUT_CALLBACK) {
                    bool keep_open = hit->on_fire
                        ? hit->on_fire(hit->id, hit->user) : false;
                    if (!keep_open) { done = true; }
                } else {
                    if (shortcuts->out_id) { *shortcuts->out_id = hit->id; }
                    done = true;
                }
                continue;
            }
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
