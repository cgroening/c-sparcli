#include "input_internal.h"
#include "internal.h"   /* sc_terminal_width, sc_utf8_string_length */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct {
    const char  *prompt;
    ScLineEditor ed;
    ScNumberOpts opts;
    bool         bounded;
} NumberState;


static double clamp(const NumberState *s, double v) {
    if (!s->bounded) { return v; }
    if (v < s->opts.min) { return s->opts.min; }
    if (v > s->opts.max) { return s->opts.max; }
    return v;
}

/** Replaces the editor content with `v` formatted to `decimals` places. */
static void set_value(NumberState *s, double v) {
    char buf[64];
    snprintf(buf, sizeof buf, "%.*f", s->opts.decimals, v);
    sc_le_free(&s->ed);
    sc_le_init(&s->ed, buf);
}

static ScRendered *number_render(void *state) {
    NumberState *s = state;
    ScText *t = sc_text_new();
    if (!t) { return NULL; }

    sc_text_append(t, s->prompt, s->opts.prompt_style);
    sc_text_append(t, " ", (ScTextStyle){ 0 });

    int field = sc_terminal_width()
              - (int)sc_utf8_string_length(s->prompt, strlen(s->prompt)) - 1;
    if (field < 1) { field = 1; }
    ScTextStyle ph = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    sc_le_render_into(&s->ed, t, s->opts.value_style, s->opts.cursor_style,
                      NULL, NULL, ph, field);

    /* Show the allowed range, when bounded. */
    if (s->bounded) {
        char range[80];
        snprintf(range, sizeof range, "  [%.*f\xe2\x80\x93%.*f]",
                 s->opts.decimals, s->opts.min, s->opts.decimals, s->opts.max);
        sc_text_append(t, range,
            (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
    }

    sc_append_hint(t, s->opts.hint ? s->opts.hint
                   : "\xe2\x86\x91/\xe2\x86\x93 adjust \xc2\xb7 enter submit \xc2\xb7 esc cancel",
                   s->opts.hide_hint, s->opts.hint_style);

    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}

static void number_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    NumberState *s = state;
    double step = s->opts.step != 0 ? s->opts.step : 1;

    switch (key.type) {
        case SC_KEY_UP:
            set_value(s, clamp(s, strtod(s->ed.buf, NULL) + step));
            return;
        case SC_KEY_DOWN:
            set_value(s, clamp(s, strtod(s->ed.buf, NULL) - step));
            return;
        case SC_KEY_ENTER:
            *done = true;
            return;
        case SC_KEY_CHAR:
            if (!sc_filter_decimal(key.codepoint, NULL)) { return; }
            break;  /* fall through to the editor for the insert */
        default:
            break;
    }
    sc_le_handle(&s->ed, key);
}

ScInputStatus sc_number_input(const char *prompt, double *out, ScNumberOpts opts) {
    if (!prompt || !out) { return SC_INPUT_ERROR; }
    sc_theme_apply_number(&opts);

    NumberState s = {
        .prompt  = prompt,
        .opts    = opts,
        .bounded = opts.max > opts.min,
    };
    char buf[64];
    snprintf(buf, sizeof buf, "%.*f", opts.decimals, clamp(&s, opts.initial));
    sc_le_init(&s.ed, buf);
    if (!s.ed.buf) { return SC_INPUT_ERROR; }

    ScPromptVTable vt = { .render = number_render, .on_key = number_on_key };
    ScInputStatus status = sc_prompt_run(&vt, &s);

    if (status == SC_INPUT_OK) {
        *out = clamp(&s, strtod(s.ed.buf, NULL));
        if (!opts.hide_summary) {
            char line[96];
            snprintf(line, sizeof line, "? %s %.*f", prompt, opts.decimals, *out);
            sc_println(line, opts.summary_style);
        }
    }
    sc_le_free(&s.ed);
    return status;
}
