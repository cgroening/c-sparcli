#include "input_internal.h"

#include <stdio.h>


typedef struct {
    const char   *question;
    const char   *yes_label;
    const char   *no_label;
    ScConfirmOpts opts;
    ScColor       accent;
    bool          value;
} ConfirmState;


/** Appends one option, highlighted (accent background) when selected. */
static void append_option(
    ScText *t, const char *label, bool selected, ScColor accent
) {
    if (selected) {
        ScTextStyle hl = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLACK, accent };
        sc_text_append(t, " ", hl);
        sc_text_append(t, label, hl);
        sc_text_append(t, " ", hl);
    } else {
        ScTextStyle dim = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        sc_text_append(t, " ", dim);
        sc_text_append(t, label, dim);
        sc_text_append(t, " ", dim);
    }
}

static ScRendered *confirm_render(void *state) {
    ConfirmState *s = state;
    ScText *t = sc_text_new();
    if (!t) { return NULL; }

    ScTextStyle q = s->opts.question_style;
    sc_text_append(t, "? ", q);
    sc_text_append(t, s->question, q);
    sc_text_append(t, "   ", (ScTextStyle){ 0 });

    append_option(t, s->yes_label, s->value,  s->accent);
    sc_text_append(t, " ", (ScTextStyle){ 0 });
    append_option(t, s->no_label, !s->value, s->accent);

    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}

static void confirm_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    ConfirmState *s = state;
    switch (key.type) {
        case SC_KEY_LEFT:
        case SC_KEY_RIGHT:
        case SC_KEY_TAB:
        case SC_KEY_BACKTAB:
            s->value = !s->value;
            return;
        case SC_KEY_ENTER:
            *done = true;
            return;
        case SC_KEY_CHAR:
            switch (key.bytes[0]) {
                case 'y': case 'Y': s->value = true;  *done = true; break;
                case 'n': case 'N': s->value = false; *done = true; break;
                case 'h': case 'l': s->value = !s->value; break;
            }
            return;
        default:
            return;
    }
}

ScInputStatus sc_confirm(const char *question, bool *out, ScConfirmOpts opts) {
    if (!question || !out) { return SC_INPUT_ERROR; }

    ConfirmState s = {
        .question  = question,
        .yes_label = opts.yes_label ? opts.yes_label : "Yes",
        .no_label  = opts.no_label  ? opts.no_label  : "No",
        .opts      = opts,
        .accent    = opts.accent.index ? opts.accent : SC_ANSI_COLOR_GREEN,
        .value     = opts.default_yes,
    };

    ScPromptVTable vt = { .render = confirm_render, .on_key = confirm_on_key };
    ScInputStatus status = sc_prompt_run(&vt, &s);

    if (status == SC_INPUT_OK) {
        *out = s.value;
        /* Persistent summary line in place of the (now-cleared) prompt. */
        printf("? %s %s\n", question, s.value ? s.yes_label : s.no_label);
    }
    return status;
}
