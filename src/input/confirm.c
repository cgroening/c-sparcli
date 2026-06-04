#include "input_internal.h"
#include "internal.h"   /* sc_terminal_width */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Render-time state for a single confirm prompt. */
typedef struct ConfirmState {
    const char *question;
    const char *yes_label;
    const char *no_label;
    ScConfirmOpts opts;
    ScColor accent;
    bool value;
} ConfirmState;

static const char *const DEFAULT_HINT =
    "\xe2\x86\x90/\xe2\x86\x92 toggle \xc2\xb7 enter confirm \xc2\xb7 "
    "esc cancel";


static ConfirmState make_state(const char *question, bool value,
                               ScConfirmOpts opts);
static ScRendered *confirm_render(void *state);
    static void append_option(ScText *text, const char *label, bool selected,
                              ScTextStyle selected_style,
                              ScTextStyle unselected_style);
static void confirm_on_key(void *state, ScKey key, bool *done, bool *cancel);


ScInputStatus sc_confirm(const char *question, bool *out, ScConfirmOpts opts) {
    if (!question || !out) {
        return SC_INPUT_ERROR;
    }
    sc_theme_apply_confirm(&opts);

    ConfirmState state = make_state(question, opts.default_yes, opts);
    ScPromptVTable vtable = {
        .render = confirm_render,
        .on_key = confirm_on_key,
    };
    ScPromptShortcuts sk = {
        opts.shortcuts, opts.n_shortcuts, opts.out_shortcut_id
    };
    ScInputStatus status =
        sc_prompt_run(&vtable, &state, opts.shortcuts ? &sk : NULL, NULL);
    if (status != SC_INPUT_OK) {
        return status;
    }

    *out = state.value;
    if (!opts.hide_summary) {
        const char *answer = state.value ? state.yes_label : state.no_label;
        size_t size = strlen(question) + strlen(answer) + 4;
        char *line = malloc(size);
        if (line) {
            snprintf(line, size, "? %s %s", question, answer);
            sc_println(line, opts.summary_style);
            free(line);
        }
    }
    return SC_INPUT_OK;
}

ScRendered *sc_confirm_frame(const char *question, bool yes,
                             ScConfirmOpts opts) {
    ConfirmState state = make_state(question, yes, opts);
    return confirm_render(&state);
}

/** Builds the state shared by the interactive prompt and the snapshot frame. */
static ConfirmState make_state(const char *question, bool value,
                               ScConfirmOpts opts) {
    return (ConfirmState){
        .question = question,
        .yes_label = opts.yes_label ? opts.yes_label : "Yes",
        .no_label = opts.no_label ? opts.no_label : "No",
        .opts = opts,
        .accent = opts.accent.index ? opts.accent : SC_ANSI_COLOR_GREEN,
        .value = value,
    };
}

static ScRendered *confirm_render(void *state) {
    ConfirmState *self = state;
    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }

    ScTextStyle selected_style = sc_style_set(self->opts.selected_style)
        ? self->opts.selected_style
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLACK, self->accent };
    ScTextStyle unselected_style = sc_style_set(self->opts.unselected_style)
        ? self->opts.unselected_style
        : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                         SC_ANSI_COLOR_NONE };

    sc_text_append(text, "? ", self->opts.prompt_style);
    sc_prompt_append(text, self->question, self->opts.prompt_style,
                     self->opts.prompt_markup, self->opts.prompt_text);
    sc_text_append(text, "   ", (ScTextStyle){ 0 });

    append_option(text, self->yes_label, self->value,
                  selected_style, unselected_style);
    sc_text_append(text, " ", (ScTextStyle){ 0 });
    append_option(text, self->no_label, !self->value,
                  selected_style, unselected_style);

    ScRendered *body = sc_capture_text(text);
    sc_text_free(text);
    body = sc_box_wrap(body, self->opts.box, SC_WIDTH_CONTENT,
                       sc_terminal_width());
    return sc_compose_hint(body,
                           self->opts.hint ? self->opts.hint : DEFAULT_HINT,
                           self->opts.hint_layout, self->opts.hint_pos,
                           self->opts.hint_style);
}

/** Appends one option, styled selected or unselected. */
static void append_option(ScText *text, const char *label, bool selected,
                          ScTextStyle selected_style,
                          ScTextStyle unselected_style) {
    ScTextStyle style = selected ? selected_style : unselected_style;
    sc_text_append(text, " ", style);
    sc_text_append(text, label, style);
    sc_text_append(text, " ", style);
}

static void confirm_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    ConfirmState *self = state;
    switch (key.type) {
        case SC_KEY_LEFT:
        case SC_KEY_RIGHT:
        case SC_KEY_TAB:
        case SC_KEY_BACKTAB:
            self->value = !self->value;
            return;
        case SC_KEY_ENTER:
            *done = true;
            return;
        case SC_KEY_CHAR:
            // Ctrl/Alt + char isn't a letter pick
            if (key.mods != 0) { return; }
            switch (key.bytes[0]) {
                case 'y': case 'Y': self->value = true;  *done = true; break;
                case 'n': case 'N': self->value = false; *done = true; break;
                case 'h': case 'l': self->value = !self->value;        break;
                default: break;   // any other character is ignored
            }
            return;
        default:
            return;
    }
}
