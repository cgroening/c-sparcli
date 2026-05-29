#include "input_internal.h"

#include <stdatomic.h>


/*
 * Guards against concurrent / nested prompts: a single process-wide terminal
 * session exists (one keyboard, one screen, process-global signal handlers), so
 * only one prompt may run at a time. The atomic claim makes a misuse from
 * another thread fail safely (SC_INPUT_ERROR) instead of corrupting state.
 */
static atomic_bool g_prompt_active = false;


/** Drives one widget's raw-mode draw/read/dispatch loop to completion. */
ScInputStatus sc_prompt_run(const ScPromptVTable *vtable, void *state) {
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
        sc_screen_draw(&screen, frame->lines, frame->line_count);
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
                vtable->on_key(state, key, &done, &cancel);
        }
    }

    sc_screen_clear(&screen);
    sc_tty_end();
    atomic_store(&g_prompt_active, false);

    if (cancel) {
        return SC_INPUT_CANCELLED;
    }
    return status;
}
