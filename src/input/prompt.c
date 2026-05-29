#include "input_internal.h"


ScInputStatus sc_prompt_run(const ScPromptVTable *vtable, void *state) {
    if (!vtable || !vtable->render || !vtable->on_key) { return SC_INPUT_ERROR; }
    if (!sc_input_available())        { return SC_INPUT_ERROR; }
    if (sc_tty_begin() != SC_INPUT_OK) { return SC_INPUT_ERROR; }

    ScScreen screen = { 0 };
    bool done = false, cancel = false;
    ScInputStatus status = SC_INPUT_OK;

    while (!done && !cancel) {
        ScRendered *frame = vtable->render(state);
        if (!frame) { status = SC_INPUT_ERROR; break; }
        sc_screen_draw(&screen, frame->lines, frame->line_count);
        sc_rendered_free(frame);

        ScKey key = sc_tty_read_key();
        if (key.type == SC_KEY_NONE) { cancel = true; break; }  /* EOF */
        if (key.type == SC_KEY_CTRL_C || key.type == SC_KEY_ESC) {
            cancel = true;
            break;
        }
        vtable->on_key(state, key, &done, &cancel);
    }

    sc_screen_clear(&screen);
    sc_tty_end();

    if (cancel)               { return SC_INPUT_CANCELLED; }
    return status;
}
