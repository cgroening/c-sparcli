#include "input_internal.h"
#include "internal.h"   /* sc_terminal_width, sc_utf8_string_length */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>    /* strncasecmp (Windows: shimmed in sc_compat.h) */
#endif


/** Render-time state for a single text/password prompt. */
typedef struct TextState {
    const char *prompt;
    const ScText *prompt_text;
    bool prompt_markup;
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
    ScBoxStyle box;
    const char *hint;
    ScHintLayout hint_layout;
    ScHintPosition hint_pos;
    ScTextStyle hint_style;
    ScCharFilter char_filter;
    void *char_filter_ctx;
    const char *const *suggestions;
    size_t n_suggestions;
    ScSuggestOpts suggest;
    bool (*validate)(const char *, void *, const char **);
    void *validate_ctx;

    /** Current validation error, or NULL. */
    const char *error;

    /** Highlighted dropdown row; -1 = none (the field itself). */
    int drop_cursor;

    /** Input history (borrowed) + auto-add resolution. */
    ScHistory *history;
    bool history_auto;

    /** Recalled history entry; -1 = editing the live line. */
    int hist_cursor;

    /** The live line preserved while walking through the history. */
    char *hist_saved;
} TextState;

/** A dropdown candidate: index into `suggestions` + its match score. */
typedef struct SuggestMatch {
    size_t index;
    int score;
} SuggestMatch;

static const char *const DEFAULT_HINT = "enter submit \xc2\xb7 esc cancel";

/** Dropdown rows shown when `ScSuggestOpts.max_visible` is left at 0. */
enum { SUGGEST_DEFAULT_VISIBLE = 5 };


static TextState state_from_cfg(const ScTextEntryCfg *cfg);
static ScRendered *text_render(void *state);
    static ScRendered *render_inline(TextState *self);
        static ScRendered *inline_body_classic(TextState *self);
        static ScRendered *inline_body_dropdown(TextState *self);
            static void append_field_line(TextState *self, ScText *text);
            static ScRendered *capture_meta_lines(TextState *self);
    static ScRendered *render_boxed(TextState *self);
        static ScText *render_boxed_inner(TextState *self, int field);
        static ScRendered *capture_boxed_panel(
            TextState *self, const ScText *inner);
        static ScRendered *stack_dropdown(TextState *self, ScRendered *top);
        static ScRendered *stack_error_line(TextState *self, ScRendered *panel);
static void text_on_key(void *state, ScKey key, bool *done, bool *cancel);
    static bool dropdown_on_key(TextState *self, ScKey key);
        static bool dropdown_dispatch(TextState *self, ScKey key,
                                      const SuggestMatch *matches, size_t shown);
        static void accept_suggestion(TextState *self, const char *suggestion);
    static void history_navigate(TextState *self, bool older);
        static void history_recall(TextState *self, const char *text);
static char *text_edit_get(void *state);
static void text_edit_set(void *state, const char *text);
    static const char *ghost_suggestion(const TextState *self);
    static const char *best_suggestion(const TextState *self);
    static const char *text_default_hint(const TextState *self);

/* Dropdown helpers */
static bool dropdown_enabled(const TextState *self);
static size_t dropdown_visible(const TextState *self);
static bool has_dropdown_matches(const TextState *self);
static size_t collect_matches(const TextState *self, SuggestMatch *out,
                              size_t cap);
    static bool suggestion_matches(const TextState *self, const char *candidate,
                                   int *score);
    static void insert_match(SuggestMatch *out, size_t *kept, size_t cap,
                             SuggestMatch match);
static ScRendered *capture_dropdown(TextState *self);
    static void append_dropdown_rows(TextState *self, ScText *rows,
                                     const SuggestMatch *matches, size_t shown,
                                     size_t total);
static ScTextStyle resolve_selected_style(const TextState *self);
static ScTextStyle resolve_more_style(const TextState *self);

static void counter_str(char *buf, size_t cap, const TextState *self,
                        bool parens);
    static size_t cp_count(const char *str);
static ScTextStyle resolve_count_style(const TextState *self);
static ScTextStyle resolve_error_style(const TextState *self);
static void print_summary(const char *prompt, const char *value,
                          const char *mask, ScTextStyle summary_style);


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
        .box             = opts.box,
        .hint            = opts.hint,
        .hint_layout     = opts.hint_layout,
        .hint_pos        = opts.hint_pos,
        .hint_style      = opts.hint_style,
        .char_filter     = opts.char_filter,
        .char_filter_ctx = opts.char_filter_ctx,
        .suggestions     = opts.suggestions,
        .n_suggestions   = opts.n_suggestions,
        .suggest         = opts.suggest,
        .validate        = opts.validate,
        .validate_ctx    = opts.validate_ctx,
        .shortcuts       = opts.shortcuts,
        .n_shortcuts     = opts.n_shortcuts,
        .out_shortcut_id = opts.out_shortcut_id,
        .prompt_text     = opts.prompt_text,
        .prompt_markup   = opts.prompt_markup,
        .external_editor = opts.external_editor,
        .editor          = opts.editor,
        .editor_key      = opts.editor_key,
        .history         = opts.history,
        .history_auto    = !opts.no_history_add
                               && sc_history_auto_add(opts.history),
    };
    return sc_text_entry(&cfg, out);
}

/* Text-entry keeps the box on state->box (not state->opts), so it defines its
   own hint-indent accessor rather than using SC_DEFINE_HINT_INDENT. */
static int text_hint_indent(void *st) {
    return sc_box_content_left(((TextState *)st)->box);
}

ScInputStatus sc_text_entry(const ScTextEntryCfg *cfg, char **out) {
    if (!cfg || (!cfg->prompt && !cfg->prompt_text) || !out) {
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
        .paste = SC_PASTE_TEXT,
        .edit_get = text_edit_get,
        .edit_set = text_edit_set,
        .hint_indent = text_hint_indent,
    };
    ScPromptShortcuts sk = {
        cfg->shortcuts, cfg->n_shortcuts, cfg->out_shortcut_id
    };
    ScPromptEditor ed = {
        cfg->external_editor, cfg->editor, cfg->editor_key, NULL  /* suffix */
    };
    ScInputStatus status = sc_prompt_run(
        &vtable, &state, cfg->shortcuts ? &sk : NULL,
        cfg->external_editor ? &ed : NULL);

    if (status == SC_INPUT_OK) {
        *out = sc_dup_str(state.ed.buf);
        if (!*out) {
            status = SC_INPUT_ERROR;
        } else if (!cfg->hide_summary) {
            char *summary_prompt = sc_prompt_plain(
                cfg->prompt, cfg->prompt_style, cfg->prompt_markup,
                cfg->prompt_text);
            print_summary(summary_prompt ? summary_prompt : "", state.ed.buf,
                          cfg->mask, cfg->summary_style);
            free(summary_prompt);
        }
    }
    sc_le_free(&state.ed);
    free(state.hist_saved);
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
        .prompt_text     = cfg->prompt_text,
        .prompt_markup   = cfg->prompt_markup,
        .placeholder     = cfg->placeholder,
        .mask            = cfg->mask,
        .prompt_style    = cfg->prompt_style,
        .value_style     = cfg->value_style,
        .cursor_style    = cfg->cursor_style,
        .error_style     = cfg->error_style,
        .count_style     = cfg->count_style,
        .hide_char_count = cfg->hide_char_count,
        .max_chars       = cfg->max_chars,
        .box             = cfg->box,
        .hint            = cfg->hint,
        .hint_layout     = cfg->hint_layout,
        .hint_pos        = cfg->hint_pos,
        .hint_style      = cfg->hint_style,
        .char_filter     = cfg->char_filter,
        .char_filter_ctx = cfg->char_filter_ctx,
        .suggestions     = cfg->suggestions,
        .n_suggestions   = cfg->n_suggestions,
        .suggest         = cfg->suggest,
        .validate        = cfg->validate,
        .validate_ctx    = cfg->validate_ctx,
        .error           = NULL,
        /* cfg->suggest_cursor is 1-based for snapshot frames; 0 = none. */
        .drop_cursor     = cfg->suggest_cursor > 0
                               ? (int)cfg->suggest_cursor - 1 : -1,
        .history         = cfg->history,
        .history_auto    = cfg->history_auto,
        .hist_cursor     = -1,
        .hist_saved      = NULL,
    };
}

static ScRendered *text_render(void *state) {
    TextState *self = state;
    return self->box.enabled ? render_boxed(self) : render_inline(self);
}

/** Inline: "prompt value", optional ghost/dropdown, counter, error, footer. */
static ScRendered *render_inline(TextState *self) {
    ScRendered *body = dropdown_enabled(self)
        ? inline_body_dropdown(self)
        : inline_body_classic(self);
    return sc_compose_hint(body,
                           self->hint ? self->hint : text_default_hint(self),
                           self->hint_layout, self->hint_pos, self->hint_style,
                           sc_box_content_left(self->box));
}

/** Classic single-block body: value line, ghost text, counter, error. */
static ScRendered *inline_body_classic(TextState *self) {
    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }

    append_field_line(self, text);

    ScTextStyle placeholder_style = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                                      SC_ANSI_COLOR_NONE };
    const char *ghost = ghost_suggestion(self);
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
    ScRendered *body = sc_capture_text(text);
    sc_text_free(text);
    return body;
}

/** Dropdown body: value line, suggestion list, then counter/error lines. */
static ScRendered *inline_body_dropdown(TextState *self) {
    ScText *field = sc_text_new();
    if (!field) {
        return NULL;
    }
    append_field_line(self, field);
    ScRendered *field_block = sc_capture_text(field);
    sc_text_free(field);

    ScRendered *dropdown = capture_dropdown(self);
    ScRendered *meta = capture_meta_lines(self);

    const ScRendered *parts[3];
    size_t count = 0;
    if (field_block) {
        parts[count++] = field_block;
    }
    if (dropdown) {
        parts[count++] = dropdown;
    }
    if (meta) {
        parts[count++] = meta;
    }

    ScRendered *body = sc_vstack(parts, count, 0);
    sc_rendered_free(field_block);
    sc_rendered_free(dropdown);
    sc_rendered_free(meta);
    return body;
}

/** Appends "prompt value" (prompt, space, line-editor window) to `text`. */
static void append_field_line(TextState *self, ScText *text) {
    sc_prompt_append(text, self->prompt, self->prompt_style,
                     self->prompt_markup, self->prompt_text);
    sc_text_append(text, " ", (ScTextStyle){ 0 });

    // Visible value window = line width − prompt − the separating space.
    int total = self->box.width > 0 ? self->box.width : sc_terminal_width();
    int prompt_w = sc_prompt_width(self->prompt, self->prompt_style,
                                   self->prompt_markup, self->prompt_text);
    int field = total - prompt_w - 1;
    if (field < 1) {
        field = 1;
    }

    ScTextStyle placeholder_style = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                                      SC_ANSI_COLOR_NONE };
    sc_le_render_into(&self->ed, text, self->value_style, self->cursor_style,
                      self->mask, self->placeholder, placeholder_style, field);
}

/** Captures the counter/error lines below the field; NULL when neither. */
static ScRendered *capture_meta_lines(TextState *self) {
    bool has_counter = !self->hide_char_count;
    if (!has_counter && !self->error) {
        return NULL;
    }
    ScText *meta = sc_text_new();
    if (!meta) {
        return NULL;
    }
    if (has_counter) {
        char buf[48];
        counter_str(buf, sizeof buf, self, false);
        sc_text_append(meta, "  ", (ScTextStyle){ 0 });
        sc_text_append(meta, buf, resolve_count_style(self));
    }
    if (self->error) {
        if (has_counter) {
            sc_text_append(meta, "\n", (ScTextStyle){ 0 });
        }
        sc_text_append(meta, "  ", (ScTextStyle){ 0 });
        sc_text_append(meta, self->error, resolve_error_style(self));
    }
    ScRendered *block = sc_capture_text(meta);
    sc_text_free(meta);
    return block;
}

/**
 * Boxed: value inside a panel (prompt = top title, counter on the bottom-right
 * border), with the error line and footer stacked below the box.
 */
static ScRendered *render_boxed(TextState *self) {
    // Clip the value to the panel interior so it stays a single line
    // (panel width − 2 borders − 2 padding).
    int panel_width = self->box.width > 0 ? self->box.width : sc_terminal_width() - 2;
    int field = panel_width - 4;
    if (field < 1) {
        field = 1;
    }

    ScText *inner = render_boxed_inner(self, field);
    if (!inner) {
        return NULL;
    }
    ScRendered *panel = capture_boxed_panel(self, inner);
    sc_text_free(inner);
    if (!panel) {
        return NULL;
    }

    // Stack the optional suggestion dropdown and validation-error line beneath
    // the box; that combined block is the body the key-hint footer is then
    // positioned around.
    panel = stack_dropdown(self, panel);
    ScRendered *body = stack_error_line(self, panel);
    return sc_compose_hint(body,
                           self->hint ? self->hint : text_default_hint(self),
                           self->hint_layout, self->hint_pos, self->hint_style,
                           sc_box_content_left(self->box));
}

/**
 * Builds the single-line box interior: the edited value (clipped to `field`
 * columns) plus the optional autocomplete ghost. NULL on allocation failure.
 */
static ScText *render_boxed_inner(TextState *self, int field) {
    ScText *inner = sc_text_new();
    if (!inner) {
        return NULL;
    }
    ScTextStyle placeholder_style = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                                      SC_ANSI_COLOR_NONE };
    sc_le_render_into(&self->ed, inner, self->value_style, self->cursor_style,
                      self->mask, self->placeholder, placeholder_style, field);
    const char *ghost = ghost_suggestion(self);
    if (ghost) {
        sc_text_append(inner, ghost + self->ed.len, placeholder_style);
    }
    return inner;
}

/**
 * Captures `inner` inside the prompt panel: the prompt becomes the top title
 * (rich, so markup/prompt_text styles survive) and the character counter the
 * bottom-right border caption.
 */
static ScRendered *capture_boxed_panel(TextState *self, const ScText *inner) {
    char counter[48];
    ScText *title_text = sc_prompt_build_bg(
        self->prompt, self->prompt_style,
        self->prompt_markup, self->prompt_text, self->box.bg
    );
    // Fill the title pad spaces (and counter below) with the box bg so the
    // border caption inherits the widget background, not the terminal default.
    ScTextStyle title_style = self->prompt_style;
    if (self->box.bg.index != 0 && title_style.bg.index == 0) {
        title_style.bg = self->box.bg;
    }
    ScPanelOpts opts = {
        .border = self->box.border,
        .bg = self->box.bg,
        .title = { .text = self->prompt, .rich_text = title_text,
                   .style = title_style,
                   .halign = SC_ALIGN_LEFT, .pad = 1, .pos = SC_POSITION_TOP },
        .padding = sc_box_padding(self->box.padding),
        .margin = self->box.margin,
        .content_align = SC_ALIGN_LEFT,
    };
    if (opts.border.type == SC_BORDER_NONE) {
        opts.border.type = SC_BORDER_ROUNDED;
    }
    if (self->box.width > 0) {
        opts.width = self->box.width;
    } else {
        opts.full_width = true;
    }
    if (!self->hide_char_count) {
        counter_str(counter, sizeof counter, self, true);
        ScTextStyle count_style = resolve_count_style(self);
        if (self->box.bg.index != 0 && count_style.bg.index == 0) {
            count_style.bg = self->box.bg;
        }
        opts.subtitle = (ScTitle){ .text = counter,
            .style = count_style, .halign = SC_ALIGN_RIGHT,
            .pad = 1, .pos = SC_POSITION_BOTTOM };
    }
    ScRendered *panel = sc_capture_panel_text(inner, opts);
    sc_text_free(title_text);
    return panel;
}

/**
 * Stacks the suggestion dropdown beneath the captured box (boxed mode).
 * Returns the combined block, or `top` unchanged when the dropdown is
 * inactive or stacking fails.
 */
static ScRendered *stack_dropdown(TextState *self, ScRendered *top) {
    return sc_stack_below(top, capture_dropdown(self));
}

/**
 * Stacks the optional validation-error line beneath the captured box. When the
 * stack succeeds it frees `panel` and returns the combined block; otherwise it
 * returns `panel` unchanged.
 */
static ScRendered *stack_error_line(TextState *self, ScRendered *panel) {
    if (!self->error) {
        return panel;
    }
    ScText *error_text = sc_text_new();
    if (!error_text) {
        return panel;
    }
    sc_text_append(error_text, "  ", (ScTextStyle){ 0 });
    sc_text_append(error_text, self->error, resolve_error_style(self));
    ScRendered *error_rendered = sc_capture_text(error_text);
    sc_text_free(error_text);
    return sc_stack_below(panel, error_rendered);
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
    // Dropdown navigation/acceptance consumes its keys before the editor.
    if (dropdown_enabled(self) && dropdown_on_key(self, key)) {
        return;
    }
    // Up/Down recall input history when no dropdown row consumed them.
    if (self->history
        && (key.type == SC_KEY_UP || key.type == SC_KEY_DOWN)) {
        history_navigate(self, key.type == SC_KEY_UP);
        return;
    }
    // Tab accepts the autocomplete ghost suggestion, if any.
    if (key.type == SC_KEY_TAB) {
        const char *suggestion = ghost_suggestion(self);
        if (suggestion) {
            accept_suggestion(self, suggestion);
        }
        return;
    }
    if (sc_le_handle(&self->ed, key)) {
        self->error = NULL;   // editing clears the previous validation error
        self->drop_cursor = -1;   // typing reshapes the dropdown: deselect
        self->hist_cursor = -1;   // editing leaves the history recall
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
        // Record the accepted line before the prompt tears down.
        if (self->history && self->history_auto) {
            sc_history_add(self->history, self->ed.buf);
        }
        *done = true;
    }
}

/**
 * Recomputes the visible dropdown matches and dispatches navigation keys
 * against them. Returns true when the key was consumed.
 */
static bool dropdown_on_key(TextState *self, ScKey key) {
    size_t cap = dropdown_visible(self);
    if (cap == 0) {
        return false;
    }
    // calloc: every slot is defined even when fewer matches than `cap`
    // are collected, so a stale cursor can never read garbage indices.
    SuggestMatch *matches = calloc(cap, sizeof *matches);
    if (!matches) {
        return false;
    }
    size_t total = collect_matches(self, matches, cap);
    size_t shown = total < cap ? total : cap;

    bool consumed = false;
    if (shown > 0) {
        consumed = dropdown_dispatch(self, key, matches, shown);
    } else {
        self->drop_cursor = -1;
    }
    free(matches);
    return consumed;
}

/** Applies one key to the dropdown; `matches`/`shown` is the visible list. */
static bool dropdown_dispatch(TextState *self, ScKey key,
                              const SuggestMatch *matches, size_t shown) {
    // The match list may have shrunk since the highlight was set.
    if (self->drop_cursor >= (int)shown) {
        self->drop_cursor = (int)shown - 1;
    }
    switch (key.type) {
        case SC_KEY_DOWN:
            if (self->drop_cursor + 1 < (int)shown) {
                self->drop_cursor++;
            }
            return true;
        case SC_KEY_UP:
            // Moving above the first row returns focus to the field (-1).
            if (self->drop_cursor >= 0) {
                self->drop_cursor--;
            }
            return true;
        case SC_KEY_TAB: {
            size_t pick = self->drop_cursor >= 0 ? (size_t)self->drop_cursor : 0;
            accept_suggestion(self, self->suggestions[matches[pick].index]);
            return true;
        }
        case SC_KEY_ENTER:
            if (self->drop_cursor >= 0) {
                size_t pick = (size_t)self->drop_cursor;
                accept_suggestion(self, self->suggestions[matches[pick].index]);
                return true;
            }
            return false;   // no row highlighted: Enter submits normally
        default:
            return false;
    }
}

/** Replaces the edited value with `suggestion` and resets dropdown state. */
static void accept_suggestion(TextState *self, const char *suggestion) {
    sc_le_free(&self->ed);
    sc_le_init(&self->ed, suggestion);
    self->error = NULL;
    self->drop_cursor = -1;
}

/**
 * Walks the attached history: Up recalls older entries, Down newer ones.
 * Walking down past the newest entry restores the preserved live line.
 */
static void history_navigate(TextState *self, bool older) {
    size_t count = sc_history_count(self->history);
    if (count == 0) {
        return;
    }

    if (older) {
        if (self->hist_cursor == -1) {
            // Leaving the live line: preserve it for the way back down
            free(self->hist_saved);
            self->hist_saved = sc_dup_str(self->ed.buf);
            self->hist_cursor = (int)count - 1;
        } else if (self->hist_cursor > 0) {
            self->hist_cursor--;
        } else {
            return;   // already at the oldest entry
        }
        history_recall(
            self, sc_history_get(self->history, (size_t)self->hist_cursor)
        );
        return;
    }

    if (self->hist_cursor == -1) {
        return;   // already editing the live line
    }
    if ((size_t)self->hist_cursor + 1 < count) {
        self->hist_cursor++;
        history_recall(
            self, sc_history_get(self->history, (size_t)self->hist_cursor)
        );
    } else {
        // Past the newest entry: back to the live line
        history_recall(self, self->hist_saved ? self->hist_saved : "");
        self->hist_cursor = -1;
    }
}

/** Replaces the edited value with a recalled history entry. */
static void history_recall(TextState *self, const char *text) {
    sc_le_free(&self->ed);
    sc_le_init(&self->ed, text);
    self->error = NULL;
    self->drop_cursor = -1;
}

/** The ghost-text suggestion; NULL in dropdown mode (no ghost is shown). */
static const char *ghost_suggestion(const TextState *self) {
    if (self->suggest.mode != SC_SUGGEST_GHOST) {
        return NULL;
    }
    return best_suggestion(self);
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

/**
 * Default key-hint. When the dropdown is open it leads with the navigation
 * keys; when a ghost suggestion is shown it leads with "tab complete" so the
 * Tab binding is discoverable; otherwise it is the plain submit/cancel hint.
 */
static const char *text_default_hint(const TextState *self) {
    if (dropdown_enabled(self) && has_dropdown_matches(self)) {
        return "\xe2\x86\x91/\xe2\x86\x93 choose \xc2\xb7 tab/enter accept"
               " \xc2\xb7 esc cancel";
    }
    if (ghost_suggestion(self)) {
        return "tab complete \xc2\xb7 enter submit \xc2\xb7 esc cancel";
    }
    return DEFAULT_HINT;
}

/* ── Dropdown suggestion list ───────────────────────────────────────────── */

/** True when the dropdown presentation is configured and has data. */
static bool dropdown_enabled(const TextState *self) {
    return self->suggest.mode == SC_SUGGEST_DROPDOWN
        && self->suggestions && self->n_suggestions > 0 && !self->mask;
}

/** Resolved viewport size: max dropdown rows shown at once. */
static size_t dropdown_visible(const TextState *self) {
    return self->suggest.max_visible > 0
        ? (size_t)self->suggest.max_visible
        : (size_t)SUGGEST_DEFAULT_VISIBLE;
}

/** True when at least one suggestion matches the current value. */
static bool has_dropdown_matches(const TextState *self) {
    if (self->ed.len == 0) {
        return false;
    }
    for (size_t i = 0; i < self->n_suggestions; i++) {
        int score = 0;
        if (self->suggestions[i]
            && suggestion_matches(self, self->suggestions[i], &score)) {
            return true;
        }
    }
    return false;
}

/**
 * Collects the best matches for the current value into `out` (at most `cap`
 * entries, best score first; insertion order breaks ties). Returns the TOTAL
 * number of matching suggestions, which may exceed `cap` - the dropdown then
 * shows a "… +N more" line.
 */
static size_t collect_matches(const TextState *self, SuggestMatch *out,
                              size_t cap) {
    if (self->ed.len == 0) {
        return 0;
    }
    size_t total = 0;
    size_t kept = 0;
    for (size_t i = 0; i < self->n_suggestions; i++) {
        const char *candidate = self->suggestions[i];
        int score = 0;
        if (!candidate || !suggestion_matches(self, candidate, &score)) {
            continue;
        }
        total++;
        insert_match(out, &kept, cap,
                     (SuggestMatch){ .index = i, .score = score });
    }
    return total;
}

/**
 * True when `candidate` matches the typed value. An exact (case-insensitive)
 * match never counts - there is nothing left to complete, and excluding it
 * lets the dropdown close after a suggestion is accepted.
 */
static bool suggestion_matches(const TextState *self, const char *candidate,
                               int *score) {
    if (strcasecmp(candidate, self->ed.buf) == 0) {
        return false;
    }
    if (self->suggest.match == SC_SUGGEST_MATCH_FUZZY) {
        return sc_fuzzy_match(self->ed.buf, candidate, score);
    }
    *score = 0;
    return strncasecmp(candidate, self->ed.buf, self->ed.len) == 0;
}

/**
 * Inserts `match` into the score-sorted (descending) `out` array, keeping at
 * most `cap` entries. Stable: equal scores keep their insertion order.
 */
static void insert_match(SuggestMatch *out, size_t *kept, size_t cap,
                         SuggestMatch match) {
    size_t pos = *kept;
    while (pos > 0 && out[pos - 1].score < match.score) {
        pos--;
    }
    if (pos >= cap) {
        return;
    }
    size_t tail = (*kept < cap ? *kept : cap - 1) - pos;
    memmove(&out[pos + 1], &out[pos], tail * sizeof *out);
    out[pos] = match;
    if (*kept < cap) {
        (*kept)++;
    }
}

/**
 * Builds the dropdown for the current value as a captured block, framed by
 * `suggest.border` when set. Returns NULL when the dropdown is inactive
 * (ghost mode, empty value, or no matching suggestion).
 */
static ScRendered *capture_dropdown(TextState *self) {
    if (!dropdown_enabled(self)) {
        return NULL;
    }
    size_t cap = dropdown_visible(self);
    if (cap == 0) {
        return NULL;
    }
    // calloc for the same reason as in dropdown_on_key: defined slots
    SuggestMatch *matches = calloc(cap, sizeof *matches);
    if (!matches) {
        return NULL;
    }
    size_t total = collect_matches(self, matches, cap);
    if (total == 0) {
        free(matches);
        return NULL;
    }
    size_t shown = total < cap ? total : cap;

    ScText *rows = sc_text_new();
    if (!rows) {
        free(matches);
        return NULL;
    }
    append_dropdown_rows(self, rows, matches, shown, total);
    free(matches);

    ScRendered *block;
    if (self->suggest.border.type != SC_BORDER_NONE) {
        ScPanelOpts panel_opts = {
            .border = self->suggest.border,
            .padding = { .left = 1, .right = 1 },
            .content_align = SC_ALIGN_LEFT,
        };
        // Boxed fields align the dropdown frame with the box width.
        if (self->box.enabled) {
            if (self->box.width > 0) {
                panel_opts.width = self->box.width;
            } else {
                panel_opts.full_width = true;
            }
        }
        block = sc_capture_panel_text(rows, panel_opts);
    } else {
        block = sc_capture_text(rows);
    }
    sc_text_free(rows);
    return block;
}

/** Appends the match rows (+ optional "… +N more" line) to `rows`. */
static void append_dropdown_rows(TextState *self, ScText *rows,
                                 const SuggestMatch *matches, size_t shown,
                                 size_t total) {
    const char *cursor_marker = self->suggest.cursor_marker
        ? self->suggest.cursor_marker : "\xe2\x80\xa3 ";   /* ‣ */
    const char *marker = self->suggest.marker ? self->suggest.marker : "  ";
    ScTextStyle selected_style = resolve_selected_style(self);

    // The highlight may point past the end after the match list shrank.
    if (self->drop_cursor >= (int)shown) {
        self->drop_cursor = (int)shown - 1;
    }

    for (size_t i = 0; i < shown; i++) {
        bool on_cursor = ((int)i == self->drop_cursor);
        ScTextStyle row_style = on_cursor ? selected_style
                                          : self->suggest.item_style;
        if (i > 0) {
            sc_text_append(rows, "\n", (ScTextStyle){ 0 });
        }
        sc_text_append(rows, on_cursor ? cursor_marker : marker, row_style);
        sc_text_append(rows, self->suggestions[matches[i].index], row_style);
    }
    if (total > shown) {
        char more[48];
        snprintf(more, sizeof more, "\xe2\x80\xa6 +%zu more", total - shown);
        sc_text_append(rows, "\n", (ScTextStyle){ 0 });
        sc_text_append(rows, marker, (ScTextStyle){ 0 });
        sc_text_append(rows, more, resolve_more_style(self));
    }
}

/** Highlighted-row style; zero-init = bold cyan. */
static ScTextStyle resolve_selected_style(const TextState *self) {
    if (sc_style_set(self->suggest.selected_style)) {
        return self->suggest.selected_style;
    }
    return (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                          SC_ANSI_COLOR_NONE };
}

/** Overflow-line ("… +N more") style; zero-init = dim. */
static ScTextStyle resolve_more_style(const TextState *self) {
    if (sc_style_set(self->suggest.more_style)) {
        return self->suggest.more_style;
    }
    return (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                          SC_ANSI_COLOR_NONE };
}

/** Formats the character counter; `parens` wraps it as "(n)" / "(n/max)". */
static void counter_str(char *buf, size_t cap, const TextState *self,
                        bool parens) {
    size_t count = cp_count(self->ed.buf);
    // Literal format strings per branch so the compiler can verify them
    // (-Wformat-nonliteral).
    if (self->max_chars > 0) {
        if (parens) {
            snprintf(buf, cap, "(%zu/%d)", count, self->max_chars);
        } else {
            snprintf(buf, cap, "%zu/%d", count, self->max_chars);
        }
        return;
    }
    if (parens) {
        snprintf(buf, cap, "(%zu)", count);
    } else {
        snprintf(buf, cap, "%zu", count);
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

/* ── Built-in character filters ─────────────────────────────────────────── */

bool sc_filter_digits(uint32_t codepoint, void *ctx) {
    (void)ctx;
    return codepoint >= '0' && codepoint <= '9';
}

bool sc_filter_decimal(uint32_t codepoint, void *ctx) {
    (void)ctx;
    return (codepoint >= '0' && codepoint <= '9')
        || codepoint == '.' || codepoint == ','
        || codepoint == '-' || codepoint == '+';
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


/* External-editor hook: hands the editor a copy of the current value. */
static char *text_edit_get(void *state) {
    TextState *self = state;
    return sc_dup_str(self->ed.buf);
}

/* External-editor hook: replaces the value with the editor result. This is a
 * single-line field, so newlines (and CRs) are collapsed to spaces and any
 * trailing whitespace (e.g. the editor's final newline) is trimmed. */
static void text_edit_set(void *state, const char *text) {
    TextState *self = state;
    char *line = sc_dup_str(text);
    if (!line) {
        return;   // keep the current value on allocation failure
    }
    for (char *p = line; *p; p++) {
        if (*p == '\n' || *p == '\r') { *p = ' '; }
    }
    size_t n = strlen(line);
    while (n > 0 && line[n - 1] == ' ') { line[--n] = '\0'; }
    sc_le_free(&self->ed);
    sc_le_init(&self->ed, line);
    self->hist_cursor = -1;   // the editor result replaces any recall
    free(line);
}
