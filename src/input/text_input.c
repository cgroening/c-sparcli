#include "input_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct {
    const char  *prompt;
    ScLineEditor ed;
    const char  *placeholder;
    const char  *mask;
    ScTextStyle  prompt_style;
    ScTextStyle  value_style;
    bool (*validate)(const char *, void *, const char **);
    void        *validate_ctx;
    const char  *error;   /* current validation error, or NULL */
} TextState;


static char *dup_str(const char *s) {
    size_t n = strlen(s);
    char  *p = malloc(n + 1);
    if (p) { memcpy(p, s, n + 1); }
    return p;
}

static ScRendered *text_render(void *state) {
    TextState *s = state;
    ScText *t = sc_text_new();
    if (!t) { return NULL; }

    sc_text_append(t, s->prompt, s->prompt_style);
    sc_text_append(t, " ", (ScTextStyle){ 0 });

    ScTextStyle ph = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    sc_le_render_into(&s->ed, t, s->value_style, s->mask, s->placeholder, ph);

    if (s->error) {
        ScTextStyle err = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE };
        sc_text_append(t, "\n", (ScTextStyle){ 0 });
        sc_text_append(t, "  ", (ScTextStyle){ 0 });
        sc_text_append(t, s->error, err);
    }

    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}

static void text_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    TextState *s = state;

    if (sc_le_handle(&s->ed, key)) {
        s->error = NULL;  /* editing clears the previous validation error */
        return;
    }
    if (key.type == SC_KEY_ENTER) {
        if (s->validate) {
            const char *msg = NULL;
            if (!s->validate(s->ed.buf, s->validate_ctx, &msg)) {
                s->error = msg ? msg : "Invalid input";
                return;
            }
        }
        *done = true;
    }
}

ScInputStatus sc_text_entry(
    const char *prompt, char **out,
    const char *initial, const char *placeholder, const char *mask,
    ScTextStyle prompt_style, ScTextStyle value_style,
    bool (*validate)(const char *, void *, const char **), void *validate_ctx
) {
    if (!prompt || !out) { return SC_INPUT_ERROR; }

    TextState s = {
        .prompt       = prompt,
        .placeholder  = placeholder,
        .mask         = mask,
        .prompt_style = prompt_style,
        .value_style  = value_style,
        .validate     = validate,
        .validate_ctx = validate_ctx,
        .error        = NULL,
    };
    sc_le_init(&s.ed, initial);
    if (!s.ed.buf) { return SC_INPUT_ERROR; }

    ScPromptVTable vt = { .render = text_render, .on_key = text_on_key };
    ScInputStatus status = sc_prompt_run(&vt, &s);

    if (status == SC_INPUT_OK) {
        *out = dup_str(s.ed.buf);
        if (!*out) { status = SC_INPUT_ERROR; }
        else if (mask) {
            printf("%s ********\n", prompt);          /* never echo a secret */
        } else {
            printf("%s %s\n", prompt, s.ed.buf);
        }
    }
    sc_le_free(&s.ed);
    return status;
}

ScInputStatus sc_text_input(const char *prompt, char **out, ScTextInputOpts opts) {
    (void)opts.width;  /* reserved for future field-width truncation */
    return sc_text_entry(
        prompt, out, opts.initial, opts.placeholder, NULL,
        opts.prompt_style, opts.value_style,
        opts.validate, opts.validate_ctx
    );
}
