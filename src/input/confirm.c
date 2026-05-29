#include "input_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct {
    const char   *question;
    const char   *yes_label;
    const char   *no_label;
    ScConfirmOpts opts;
    ScColor       accent;
    bool          value;
} ConfirmState;


/** Appends one option with the given selected/unselected style. */
static void append_option(
    ScText *t, const char *label, bool selected,
    ScTextStyle sel, ScTextStyle unsel
) {
    ScTextStyle st = selected ? sel : unsel;
    sc_text_append(t, " ", st);
    sc_text_append(t, label, st);
    sc_text_append(t, " ", st);
}

static ScRendered *confirm_render(void *state) {
    ConfirmState *s = state;
    ScText *t = sc_text_new();
    if (!t) { return NULL; }

    ScTextStyle sel = sc_style_set(s->opts.selected_style)
        ? s->opts.selected_style
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLACK, s->accent };
    ScTextStyle unsel = sc_style_set(s->opts.unselected_style)
        ? s->opts.unselected_style
        : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };

    ScTextStyle q = s->opts.prompt_style;
    sc_text_append(t, "? ", q);
    sc_text_append(t, s->question, q);
    sc_text_append(t, "   ", (ScTextStyle){ 0 });

    append_option(t, s->yes_label, s->value,  sel, unsel);
    sc_text_append(t, " ", (ScTextStyle){ 0 });
    append_option(t, s->no_label, !s->value, sel, unsel);

    sc_append_hint(t, s->opts.hint ? s->opts.hint
                   : "\xe2\x86\x90/\xe2\x86\x92 toggle \xc2\xb7 enter confirm \xc2\xb7 esc cancel",
                   s->opts.hide_hint, s->opts.hint_style);

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

/** Builds the state shared by the interactive prompt and the snapshot frame. */
static ConfirmState make_state(const char *question, bool value, ScConfirmOpts opts) {
    return (ConfirmState){
        .question  = question,
        .yes_label = opts.yes_label ? opts.yes_label : "Yes",
        .no_label  = opts.no_label  ? opts.no_label  : "No",
        .opts      = opts,
        .accent    = opts.accent.index ? opts.accent : SC_ANSI_COLOR_GREEN,
        .value     = value,
    };
}

ScInputStatus sc_confirm(const char *question, bool *out, ScConfirmOpts opts) {
    if (!question || !out) { return SC_INPUT_ERROR; }
    sc_theme_apply_confirm(&opts);

    ConfirmState s = make_state(question, opts.default_yes, opts);
    ScPromptVTable vt = { .render = confirm_render, .on_key = confirm_on_key };
    ScInputStatus status = sc_prompt_run(&vt, &s);

    if (status == SC_INPUT_OK) {
        *out = s.value;
        if (!opts.hide_summary) {
            const char *ans = s.value ? s.yes_label : s.no_label;
            size_t n = strlen(question) + strlen(ans) + 4;
            char  *line = malloc(n);
            if (line) {
                snprintf(line, n, "? %s %s", question, ans);
                sc_println(line, opts.summary_style);
                free(line);
            }
        }
    }
    return status;
}

ScRendered *sc_confirm_frame(const char *question, bool yes, ScConfirmOpts opts) {
    ConfirmState s = make_state(question, yes, opts);
    return confirm_render(&s);
}
