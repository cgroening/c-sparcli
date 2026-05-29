#include "input_internal.h"


/* Guards against nested prompts: a single global terminal session exists, so a
 * widget invoked from inside another widget's loop would corrupt it. */
static bool g_prompt_active = false;


ScInputStatus sc_prompt_run(const ScPromptVTable *vtable, void *state) {
    if (!vtable || !vtable->render || !vtable->on_key) { return SC_INPUT_ERROR; }
    if (g_prompt_active)               { return SC_INPUT_ERROR; }  /* no nesting */
    if (!sc_input_available())         { return SC_INPUT_ERROR; }
    if (sc_tty_begin() != SC_INPUT_OK) { return SC_INPUT_ERROR; }
    g_prompt_active = true;

    ScScreen screen = { 0 };
    bool done = false, cancel = false;
    ScInputStatus status = SC_INPUT_OK;

    while (!done && !cancel) {
        ScRendered *frame = vtable->render(state);
        if (!frame) { status = SC_INPUT_ERROR; break; }
        sc_screen_draw(&screen, frame->lines, frame->line_count);
        sc_rendered_free(frame);

        ScKey key = sc_tty_read_key();
        switch (key.type) {
            case SC_KEY_NONE:    cancel = true; continue;  /* EOF / read error */
            case SC_KEY_CTRL_C:
            case SC_KEY_ESC:     cancel = true; continue;
            case SC_KEY_RESIZE:
                /* Terminal reflowed: drop the stale region and repaint fresh. */
                sc_screen_clear(&screen);
                continue;
            default:
                vtable->on_key(state, key, &done, &cancel);
        }
    }

    sc_screen_clear(&screen);
    sc_tty_end();
    g_prompt_active = false;

    if (cancel) { return SC_INPUT_CANCELLED; }
    return status;
}
