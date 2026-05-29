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
    ScTextStyle  cursor_style;
    ScTextStyle  error_style;
    ScTextStyle  count_style;
    bool         hide_char_count;
    int          max_chars;
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

/** Counts UTF-8 codepoints (not bytes) in `s`. */
static size_t cp_count(const char *s) {
    size_t n = 0;
    for (; *s; s++) {
        if (((unsigned char)*s & 0xC0) != 0x80) { n++; }
    }
    return n;
}

static ScRendered *text_render(void *state) {
    TextState *s = state;
    ScText *t = sc_text_new();
    if (!t) { return NULL; }

    sc_text_append(t, s->prompt, s->prompt_style);
    sc_text_append(t, " ", (ScTextStyle){ 0 });

    ScTextStyle ph = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    sc_le_render_into(&s->ed, t, s->value_style, s->cursor_style, s->mask,
                      s->placeholder, ph);

    /* Character counter, on its own line below the field. */
    if (!s->hide_char_count) {
        char buf[48];
        size_t n = cp_count(s->ed.buf);
        if (s->max_chars > 0) {
            snprintf(buf, sizeof buf, "%zu/%d", n, s->max_chars);
        } else {
            snprintf(buf, sizeof buf, "%zu", n);
        }
        ScTextStyle cs = sc_style_set(s->count_style)
            ? s->count_style
            : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        sc_text_append(t, "\n", (ScTextStyle){ 0 });
        sc_text_append(t, "  ", (ScTextStyle){ 0 });
        sc_text_append(t, buf, cs);
    }

    if (s->error) {
        ScTextStyle err = sc_style_set(s->error_style)
            ? s->error_style
            : (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE };
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

    /* Enforce the character cap: ignore further printable input at the limit. */
    if (key.type == SC_KEY_CHAR && s->max_chars > 0
        && cp_count(s->ed.buf) >= (size_t)s->max_chars) {
        return;
    }
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

/** Fills a TextState from `cfg` (editor not yet initialized). */
static TextState state_from_cfg(const ScTextEntryCfg *cfg) {
    return (TextState){
        .prompt          = cfg->prompt,
        .placeholder     = cfg->placeholder,
        .mask            = cfg->mask,
        .prompt_style    = cfg->prompt_style,
        .value_style     = cfg->value_style,
        .cursor_style    = cfg->cursor_style,
        .error_style     = cfg->error_style,
        .count_style     = cfg->count_style,
        .hide_char_count = cfg->hide_char_count,
        .max_chars       = cfg->max_chars,
        .validate        = cfg->validate,
        .validate_ctx    = cfg->validate_ctx,
        .error           = NULL,
    };
}

/** Prints the persistent summary line (value masked for secrets). */
static void print_summary(
    const char *prompt, const char *value, const char *mask,
    ScTextStyle summary_style
) {
    const char *shown = mask ? "********" : value;
    size_t n = strlen(prompt) + strlen(shown) + 2;
    char  *line = malloc(n);
    if (!line) { return; }
    snprintf(line, n, "%s %s", prompt, shown);
    sc_println(line, summary_style);
    free(line);
}

ScInputStatus sc_text_entry(const ScTextEntryCfg *cfg, char **out) {
    if (!cfg || !cfg->prompt || !out) { return SC_INPUT_ERROR; }

    TextState s = state_from_cfg(cfg);
    sc_le_init(&s.ed, cfg->initial);
    if (!s.ed.buf) { return SC_INPUT_ERROR; }

    ScPromptVTable vt = { .render = text_render, .on_key = text_on_key };
    ScInputStatus status = sc_prompt_run(&vt, &s);

    if (status == SC_INPUT_OK) {
        *out = dup_str(s.ed.buf);
        if (!*out) { status = SC_INPUT_ERROR; }
        else if (!cfg->hide_summary) {
            print_summary(cfg->prompt, s.ed.buf, cfg->mask, cfg->summary_style);
        }
    }
    sc_le_free(&s.ed);
    return status;
}

ScRendered *sc_text_entry_frame(const ScTextEntryCfg *cfg) {
    TextState s = state_from_cfg(cfg);
    sc_le_init(&s.ed, cfg->initial);
    ScRendered *r = text_render(&s);
    sc_le_free(&s.ed);
    return r;
}

ScInputStatus sc_text_input(const char *prompt, char **out, ScTextInputOpts opts) {
    (void)opts.width;  /* reserved for future field-width truncation */
    ScTextEntryCfg cfg = {
        .prompt          = prompt,
        .initial         = opts.initial,
        .placeholder     = opts.placeholder,
        .mask            = NULL,
        .prompt_style    = opts.prompt_style,
        .value_style     = opts.value_style,
        .cursor_style    = opts.cursor_style,
        .error_style     = opts.error_style,
        .count_style     = opts.count_style,
        .summary_style   = opts.summary_style,
        .hide_summary    = opts.hide_summary,
        .hide_char_count = opts.hide_char_count,
        .max_chars       = opts.max_chars,
        .validate        = opts.validate,
        .validate_ctx    = opts.validate_ctx,
    };
    return sc_text_entry(&cfg, out);
}
