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
    bool         boxed;
    ScBorderStyle border;
    int          width;
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

/** Formats the character counter; `parens` wraps it as "(n)"/"(n/max)". */
static void counter_str(char *buf, size_t cap, const TextState *s, bool parens) {
    size_t n = cp_count(s->ed.buf);
    const char *fmt_count = parens ? "(%zu)"    : "%zu";
    const char *fmt_max   = parens ? "(%zu/%d)" : "%zu/%d";
    if (s->max_chars > 0) { snprintf(buf, cap, fmt_max, n, s->max_chars); }
    else                  { snprintf(buf, cap, fmt_count, n); }
}

static ScTextStyle resolve_count_style(const TextState *s) {
    return sc_style_set(s->count_style)
        ? s->count_style
        : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
}

static ScTextStyle resolve_error_style(const TextState *s) {
    return sc_style_set(s->error_style)
        ? s->error_style
        : (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE };
}

/** Inline rendering: "prompt value", counter line, optional error line. */
static ScRendered *render_inline(TextState *s) {
    ScText *t = sc_text_new();
    if (!t) { return NULL; }

    sc_text_append(t, s->prompt, s->prompt_style);
    sc_text_append(t, " ", (ScTextStyle){ 0 });

    ScTextStyle ph = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    sc_le_render_into(&s->ed, t, s->value_style, s->cursor_style, s->mask,
                      s->placeholder, ph);

    if (!s->hide_char_count) {
        char buf[48];
        counter_str(buf, sizeof buf, s, false);
        sc_text_append(t, "\n", (ScTextStyle){ 0 });
        sc_text_append(t, "  ", (ScTextStyle){ 0 });
        sc_text_append(t, buf, resolve_count_style(s));
    }
    if (s->error) {
        sc_text_append(t, "\n", (ScTextStyle){ 0 });
        sc_text_append(t, "  ", (ScTextStyle){ 0 });
        sc_text_append(t, s->error, resolve_error_style(s));
    }

    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}

/** Boxed rendering: value inside a panel, prompt as top title, counter on the
 *  bottom-right border, optional error line stacked below the box. */
static ScRendered *render_boxed(TextState *s) {
    ScText *inner = sc_text_new();
    if (!inner) { return NULL; }
    ScTextStyle ph = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    sc_le_render_into(&s->ed, inner, s->value_style, s->cursor_style, s->mask,
                      s->placeholder, ph);

    char counter[48];
    ScPanelOpts opts = {
        .border        = s->border,
        .title         = { .text = s->prompt, .style = s->prompt_style,
                           .align = SC_ALIGN_LEFT, .pad = 1, .pos = SC_POSITION_TOP },
        .padding       = { .left = 1, .right = 1 },
        .content_align = SC_ALIGN_LEFT,
    };
    if (opts.border.type == SC_BORDER_NONE) { opts.border.type = SC_BORDER_ROUNDED; }
    if (s->width > 0) { opts.width = s->width; } else { opts.full_width = true; }
    if (!s->hide_char_count) {
        counter_str(counter, sizeof counter, s, true);
        opts.subtitle = (ScTitle){ .text = counter, .style = resolve_count_style(s),
                                   .align = SC_ALIGN_RIGHT, .pad = 1,
                                   .pos = SC_POSITION_BOTTOM };
    }

    ScRendered *panel = sc_capture_panel_text(inner, opts);
    sc_text_free(inner);
    if (!panel || !s->error) { return panel; }

    /* Stack the validation error beneath the box. */
    ScText *et = sc_text_new();
    if (!et) { return panel; }
    sc_text_append(et, "  ", (ScTextStyle){ 0 });
    sc_text_append(et, s->error, resolve_error_style(s));
    ScRendered *er = sc_capture_text(et);
    sc_text_free(et);
    if (!er) { return panel; }

    const ScRendered *parts[2] = { panel, er };
    ScRendered *stacked = sc_vstack(parts, 2, 0);
    sc_rendered_free(panel);
    sc_rendered_free(er);
    return stacked;
}

static ScRendered *text_render(void *state) {
    TextState *s = state;
    return s->boxed ? render_boxed(s) : render_inline(s);
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
        .boxed           = cfg->boxed,
        .border          = cfg->border,
        .width           = cfg->width,
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
        .boxed           = opts.boxed,
        .border          = opts.border,
        .width           = opts.width,
        .validate        = opts.validate,
        .validate_ctx    = opts.validate_ctx,
    };
    return sc_text_entry(&cfg, out);
}
