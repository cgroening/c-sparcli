#include "input_internal.h"
#include "internal.h"   /* sc_terminal_width, sc_utf8_string_length */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>    /* strncasecmp */


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
    const char  *hint;
    bool         hide_hint;
    ScTextStyle  hint_style;
    ScCharFilter char_filter;
    void        *char_filter_ctx;
    const char *const *suggestions;
    size_t       n_suggestions;
    bool (*validate)(const char *, void *, const char **);
    void        *validate_ctx;
    const char  *error;   /* current validation error, or NULL */
} TextState;


/** Returns the first suggestion that has the current value as a (case-
 *  insensitive) strict prefix, or NULL. */
static const char *best_suggestion(const TextState *s) {
    if (s->ed.len == 0 || !s->suggestions) { return NULL; }
    for (size_t i = 0; i < s->n_suggestions; i++) {
        const char *sug = s->suggestions[i];
        if (sug && strlen(sug) > s->ed.len
            && strncasecmp(sug, s->ed.buf, s->ed.len) == 0) {
            return sug;
        }
    }
    return NULL;
}


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

    /* Visible value window = line width − prompt − the separating space. */
    int total = s->width > 0 ? s->width : sc_terminal_width();
    int prompt_w = (int)sc_utf8_string_length(s->prompt, strlen(s->prompt));
    int field = total - prompt_w - 1;
    if (field < 1) { field = 1; }

    ScTextStyle ph = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    sc_le_render_into(&s->ed, t, s->value_style, s->cursor_style, s->mask,
                      s->placeholder, ph, field);

    const char *ghost = best_suggestion(s);
    if (ghost) { sc_text_append(t, ghost + s->ed.len, ph); }  /* dim completion */

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
    sc_append_hint(t, s->hint ? s->hint : "enter submit \xc2\xb7 esc cancel",
                   s->hide_hint, s->hint_style);

    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}

/** Boxed rendering: value inside a panel, prompt as top title, counter on the
 *  bottom-right border, optional error line stacked below the box. */
static ScRendered *render_boxed(TextState *s) {
    ScText *inner = sc_text_new();
    if (!inner) { return NULL; }

    /* Clip the value to the panel's interior so it stays a single line
     * (panel width − 2 borders − 2 padding). */
    int panel_w = s->width > 0 ? s->width : sc_terminal_width() - 2;
    int field = panel_w - 4;
    if (field < 1) { field = 1; }

    ScTextStyle ph = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    sc_le_render_into(&s->ed, inner, s->value_style, s->cursor_style, s->mask,
                      s->placeholder, ph, field);

    const char *ghost = best_suggestion(s);
    if (ghost) { sc_text_append(inner, ghost + s->ed.len, ph); }  /* dim completion */

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
    if (!panel) { return NULL; }

    /* Stack an optional error line and the key-hint footer beneath the box. */
    ScRendered *er = NULL;
    if (s->error) {
        ScText *et = sc_text_new();
        if (et) {
            sc_text_append(et, "  ", (ScTextStyle){ 0 });
            sc_text_append(et, s->error, resolve_error_style(s));
            er = sc_capture_text(et);
            sc_text_free(et);
        }
    }
    ScRendered *foot = NULL;
    if (!s->hide_hint) {
        ScText *ft = sc_text_new();
        if (ft) {
            ScTextStyle hs = sc_style_set(s->hint_style) ? s->hint_style
                : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
            sc_text_append(ft, s->hint ? s->hint : "enter submit \xc2\xb7 esc cancel", hs);
            foot = sc_capture_text(ft);
            sc_text_free(ft);
        }
    }
    if (!er && !foot) { return panel; }

    const ScRendered *parts[3];
    size_t n = 0;
    parts[n++] = panel;
    if (er)   { parts[n++] = er; }
    if (foot) { parts[n++] = foot; }
    ScRendered *stacked = sc_vstack(parts, n, 0);
    sc_rendered_free(panel);
    sc_rendered_free(er);
    sc_rendered_free(foot);
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
    /* Apply the format filter: reject disallowed printable characters. */
    if (key.type == SC_KEY_CHAR && s->char_filter
        && !s->char_filter(key.codepoint, s->char_filter_ctx)) {
        return;
    }
    /* Tab accepts the autocomplete suggestion, if any. */
    if (key.type == SC_KEY_TAB) {
        const char *sug = best_suggestion(s);
        if (sug) { sc_le_free(&s->ed); sc_le_init(&s->ed, sug); s->error = NULL; }
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
        .hint            = cfg->hint,
        .hide_hint       = cfg->hide_hint,
        .hint_style      = cfg->hint_style,
        .char_filter     = cfg->char_filter,
        .char_filter_ctx = cfg->char_filter_ctx,
        .suggestions     = cfg->suggestions,
        .n_suggestions   = cfg->n_suggestions,
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

/* ── Built-in character filters ─────────────────────────────────────────── */

bool sc_filter_digits(uint32_t c, void *ctx) {
    (void)ctx; return c >= '0' && c <= '9';
}
bool sc_filter_decimal(uint32_t c, void *ctx) {
    (void)ctx; return (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+';
}
bool sc_filter_alpha(uint32_t c, void *ctx) {
    (void)ctx; return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
bool sc_filter_alnum(uint32_t c, void *ctx) {
    (void)ctx;
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
bool sc_filter_no_space(uint32_t c, void *ctx) {
    (void)ctx; return c != ' ' && c != '\t';
}


ScInputStatus sc_text_input(const char *prompt, char **out, ScTextInputOpts opts) {
    sc_theme_apply_text(&opts);
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
        .hint            = opts.hint,
        .hide_hint       = opts.hide_hint,
        .hint_style      = opts.hint_style,
        .char_filter     = opts.char_filter,
        .char_filter_ctx = opts.char_filter_ctx,
        .suggestions     = opts.suggestions,
        .n_suggestions   = opts.n_suggestions,
        .validate        = opts.validate,
        .validate_ctx    = opts.validate_ctx,
    };
    return sc_text_entry(&cfg, out);
}
