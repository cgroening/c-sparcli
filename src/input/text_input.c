#include "input_internal.h"
#include "internal.h"   /* sc_terminal_width, sc_utf8_string_length */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>    /* strncasecmp */


/** Render-time state for a single text/password prompt. */
typedef struct TextState {
    const char *prompt;
    ScLineEditor ed;
    const char *placeholder;
    const char *mask;
    ScTextStyle prompt_style;
    ScTextStyle value_style;
    ScTextStyle cursor_style;
    ScTextStyle error_style;
    ScTextStyle count_style;
    bool hide_char_count;
    int max_chars;
    bool boxed;
    ScBorderStyle border;
    int width;
    const char *hint;
    bool hide_hint;
    ScTextStyle hint_style;
    ScCharFilter char_filter;
    void *char_filter_ctx;
    const char *const *suggestions;
    size_t n_suggestions;
    bool (*validate)(const char *, void *, const char **);
    void *validate_ctx;

    /** Current validation error, or NULL. */
    const char *error;
} TextState;

static const char *const DEFAULT_HINT = "enter submit \xc2\xb7 esc cancel";


static TextState state_from_cfg(const ScTextEntryCfg *cfg);
static ScRendered *text_render(void *state);
    static ScRendered *render_inline(TextState *self);
    static ScRendered *render_boxed(TextState *self);
static void text_on_key(void *state, ScKey key, bool *done, bool *cancel);
    static const char *best_suggestion(const TextState *self);
static void counter_str(char *buf, size_t cap, const TextState *self,
                        bool parens);
    static size_t cp_count(const char *str);
static ScTextStyle resolve_count_style(const TextState *self);
static ScTextStyle resolve_error_style(const TextState *self);
static void print_summary(const char *prompt, const char *value,
                          const char *mask, ScTextStyle summary_style);
static char *dup_str(const char *str);


ScInputStatus sc_text_input(const char *prompt, char **out,
                            ScTextInputOpts opts) {
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

ScInputStatus sc_text_entry(const ScTextEntryCfg *cfg, char **out) {
    if (!cfg || !cfg->prompt || !out) {
        return SC_INPUT_ERROR;
    }

    TextState state = state_from_cfg(cfg);
    sc_le_init(&state.ed, cfg->initial);
    if (!state.ed.buf) {
        return SC_INPUT_ERROR;
    }

    ScPromptVTable vtable = {
        .render = text_render,
        .on_key = text_on_key,
    };
    ScInputStatus status = sc_prompt_run(&vtable, &state);

    if (status == SC_INPUT_OK) {
        *out = dup_str(state.ed.buf);
        if (!*out) {
            status = SC_INPUT_ERROR;
        } else if (!cfg->hide_summary) {
            print_summary(cfg->prompt, state.ed.buf, cfg->mask,
                          cfg->summary_style);
        }
    }
    sc_le_free(&state.ed);
    return status;
}

ScRendered *sc_text_entry_frame(const ScTextEntryCfg *cfg) {
    TextState state = state_from_cfg(cfg);
    sc_le_init(&state.ed, cfg->initial);
    ScRendered *rendered = text_render(&state);
    sc_le_free(&state.ed);
    return rendered;
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

static ScRendered *text_render(void *state) {
    TextState *self = state;
    return self->boxed ? render_boxed(self) : render_inline(self);
}

/** Inline: "prompt value", optional ghost, counter line, error line, footer. */
static ScRendered *render_inline(TextState *self) {
    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }

    sc_text_append(text, self->prompt, self->prompt_style);
    sc_text_append(text, " ", (ScTextStyle){ 0 });

    // Visible value window = line width − prompt − the separating space.
    int total = self->width > 0 ? self->width : sc_terminal_width();
    int prompt_w = (int)sc_utf8_string_length(self->prompt, strlen(self->prompt));
    int field = total - prompt_w - 1;
    if (field < 1) {
        field = 1;
    }

    ScTextStyle placeholder_style = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                                      SC_ANSI_COLOR_NONE };
    sc_le_render_into(&self->ed, text, self->value_style, self->cursor_style,
                      self->mask, self->placeholder, placeholder_style, field);

    const char *ghost = best_suggestion(self);
    if (ghost) {
        sc_text_append(text, ghost + self->ed.len, placeholder_style);
    }

    if (!self->hide_char_count) {
        char buf[48];
        counter_str(buf, sizeof buf, self, false);
        sc_text_append(text, "\n", (ScTextStyle){ 0 });
        sc_text_append(text, "  ", (ScTextStyle){ 0 });
        sc_text_append(text, buf, resolve_count_style(self));
    }
    if (self->error) {
        sc_text_append(text, "\n", (ScTextStyle){ 0 });
        sc_text_append(text, "  ", (ScTextStyle){ 0 });
        sc_text_append(text, self->error, resolve_error_style(self));
    }
    sc_append_hint(text, self->hint ? self->hint : DEFAULT_HINT,
                   self->hide_hint, self->hint_style);

    ScRendered *rendered = sc_capture_text(text);
    sc_text_free(text);
    return rendered;
}

/**
 * Boxed: value inside a panel (prompt = top title, counter on the bottom-right
 * border), with the error line and footer stacked below the box.
 */
static ScRendered *render_boxed(TextState *self) {
    ScText *inner = sc_text_new();
    if (!inner) {
        return NULL;
    }

    // Clip the value to the panel interior so it stays a single line
    // (panel width − 2 borders − 2 padding).
    int panel_width = self->width > 0 ? self->width : sc_terminal_width() - 2;
    int field = panel_width - 4;
    if (field < 1) {
        field = 1;
    }

    ScTextStyle placeholder_style = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                                      SC_ANSI_COLOR_NONE };
    sc_le_render_into(&self->ed, inner, self->value_style, self->cursor_style,
                      self->mask, self->placeholder, placeholder_style, field);

    const char *ghost = best_suggestion(self);
    if (ghost) {
        sc_text_append(inner, ghost + self->ed.len, placeholder_style);
    }

    char counter[48];
    ScPanelOpts opts = {
        .border = self->border,
        .title = { .text = self->prompt, .style = self->prompt_style,
                   .halign = SC_ALIGN_LEFT, .pad = 1, .pos = SC_POSITION_TOP },
        .padding = { .left = 1, .right = 1 },
        .content_align = SC_ALIGN_LEFT,
    };
    if (opts.border.type == SC_BORDER_NONE) {
        opts.border.type = SC_BORDER_ROUNDED;
    }
    if (self->width > 0) {
        opts.width = self->width;
    } else {
        opts.full_width = true;
    }
    if (!self->hide_char_count) {
        counter_str(counter, sizeof counter, self, true);
        opts.subtitle = (ScTitle){ .text = counter,
            .style = resolve_count_style(self), .halign = SC_ALIGN_RIGHT,
            .pad = 1, .pos = SC_POSITION_BOTTOM };
    }

    ScRendered *panel = sc_capture_panel_text(inner, opts);
    sc_text_free(inner);
    if (!panel) {
        return NULL;
    }

    // Stack an optional error line and the key-hint footer beneath the box.
    ScRendered *error_rendered = NULL;
    if (self->error) {
        ScText *error_text = sc_text_new();
        if (error_text) {
            sc_text_append(error_text, "  ", (ScTextStyle){ 0 });
            sc_text_append(error_text, self->error, resolve_error_style(self));
            error_rendered = sc_capture_text(error_text);
            sc_text_free(error_text);
        }
    }
    ScRendered *footer = NULL;
    if (!self->hide_hint) {
        ScText *footer_text = sc_text_new();
        if (footer_text) {
            ScTextStyle hint_style = sc_style_set(self->hint_style)
                ? self->hint_style
                : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                                 SC_ANSI_COLOR_NONE };
            sc_text_append(footer_text, self->hint ? self->hint : DEFAULT_HINT,
                           hint_style);
            footer = sc_capture_text(footer_text);
            sc_text_free(footer_text);
        }
    }
    if (!error_rendered && !footer) {
        return panel;
    }

    const ScRendered *parts[3];
    size_t count = 0;
    parts[count++] = panel;
    if (error_rendered) {
        parts[count++] = error_rendered;
    }
    if (footer) {
        parts[count++] = footer;
    }
    ScRendered *stacked = sc_vstack(parts, count, 0);
    sc_rendered_free(panel);
    sc_rendered_free(error_rendered);
    sc_rendered_free(footer);
    return stacked;
}

static void text_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    TextState *self = state;

    // Enforce the character cap: ignore further printable input at the limit.
    if (key.type == SC_KEY_CHAR && self->max_chars > 0
        && cp_count(self->ed.buf) >= (size_t)self->max_chars) {
        return;
    }
    // Apply the format filter: reject disallowed printable characters.
    if (key.type == SC_KEY_CHAR && self->char_filter
        && !self->char_filter(key.codepoint, self->char_filter_ctx)) {
        return;
    }
    // Tab accepts the autocomplete suggestion, if any.
    if (key.type == SC_KEY_TAB) {
        const char *suggestion = best_suggestion(self);
        if (suggestion) {
            sc_le_free(&self->ed);
            sc_le_init(&self->ed, suggestion);
            self->error = NULL;
        }
        return;
    }
    if (sc_le_handle(&self->ed, key)) {
        self->error = NULL;   // editing clears the previous validation error
        return;
    }
    if (key.type == SC_KEY_ENTER) {
        if (self->validate) {
            const char *message = NULL;
            if (!self->validate(self->ed.buf, self->validate_ctx, &message)) {
                self->error = message ? message : "Invalid input";
                return;
            }
        }
        *done = true;
    }
}

/**
 * Returns the first suggestion that has the current value as a (case-
 * insensitive) strict prefix, or NULL.
 */
static const char *best_suggestion(const TextState *self) {
    if (self->ed.len == 0 || !self->suggestions) {
        return NULL;
    }
    for (size_t i = 0; i < self->n_suggestions; i++) {
        const char *suggestion = self->suggestions[i];
        if (suggestion && strlen(suggestion) > self->ed.len
            && strncasecmp(suggestion, self->ed.buf, self->ed.len) == 0) {
            return suggestion;
        }
    }
    return NULL;
}

/** Formats the character counter; `parens` wraps it as "(n)" / "(n/max)". */
static void counter_str(char *buf, size_t cap, const TextState *self,
                        bool parens) {
    size_t count = cp_count(self->ed.buf);
    const char *fmt_count = parens ? "(%zu)" : "%zu";
    const char *fmt_max = parens ? "(%zu/%d)" : "%zu/%d";
    if (self->max_chars > 0) {
        snprintf(buf, cap, fmt_max, count, self->max_chars);
    } else {
        snprintf(buf, cap, fmt_count, count);
    }
}

/** Counts UTF-8 codepoints (not bytes) in `str`. */
static size_t cp_count(const char *str) {
    size_t count = 0;
    for (; *str; str++) {
        if (((unsigned char)*str & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

static ScTextStyle resolve_count_style(const TextState *self) {
    if (sc_style_set(self->count_style)) {
        return self->count_style;
    }
    return (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                          SC_ANSI_COLOR_NONE };
}

static ScTextStyle resolve_error_style(const TextState *self) {
    if (sc_style_set(self->error_style)) {
        return self->error_style;
    }
    return (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED,
                          SC_ANSI_COLOR_NONE };
}

/** Prints the persistent summary line (value masked for secrets). */
static void print_summary(const char *prompt, const char *value,
                          const char *mask, ScTextStyle summary_style) {
    const char *shown = mask ? "********" : value;
    size_t size = strlen(prompt) + strlen(shown) + 2;
    char *line = malloc(size);
    if (!line) {
        return;
    }
    snprintf(line, size, "%s %s", prompt, shown);
    sc_println(line, summary_style);
    free(line);
}

static char *dup_str(const char *str) {
    size_t size = strlen(str) + 1;
    char *copy = malloc(size);
    if (copy) {
        memcpy(copy, str, size);
    }
    return copy;
}


/* ── Built-in character filters ─────────────────────────────────────────── */

bool sc_filter_digits(uint32_t codepoint, void *ctx) {
    (void)ctx;
    return codepoint >= '0' && codepoint <= '9';
}

bool sc_filter_decimal(uint32_t codepoint, void *ctx) {
    (void)ctx;
    return (codepoint >= '0' && codepoint <= '9')
        || codepoint == '.' || codepoint == '-' || codepoint == '+';
}

bool sc_filter_alpha(uint32_t codepoint, void *ctx) {
    (void)ctx;
    return (codepoint >= 'A' && codepoint <= 'Z')
        || (codepoint >= 'a' && codepoint <= 'z');
}

bool sc_filter_alnum(uint32_t codepoint, void *ctx) {
    (void)ctx;
    return (codepoint >= '0' && codepoint <= '9')
        || (codepoint >= 'A' && codepoint <= 'Z')
        || (codepoint >= 'a' && codepoint <= 'z');
}

bool sc_filter_no_space(uint32_t codepoint, void *ctx) {
    (void)ctx;
    return codepoint != ' ' && codepoint != '\t';
}
